extends Node
class_name RiveAssetLoader

signal asset_resolved(metadata: Dictionary)
signal completed(error: Error)
signal _response_received(job: int, response: Array)

@export var rive_file: RiveFile
@export var allowed_https_hosts: PackedStringArray = []
@export_range(1, 256, 1) var max_download_megabytes := 32
@export_range(1, 120, 1) var timeout_seconds := 20
@export var watch_dependencies := false
var url_resolver: Callable
var resource_resolver: Callable
var _job := 0
var _running := false
var _request: HTTPRequest
var _watch_clock := 0.0
var _dependency_times: Dictionary = {}
var _stopping := false

func cancel() -> void:
	var cancelled_job := _job
	var was_running := _running
	_job += 1
	if is_instance_valid(_request):
		_request.cancel_request()
		_request.queue_free()
	_request = null
	_running = false
	if was_running: completed.emit(ERR_SKIP)
	_response_received.emit(cancelled_job, [])

func _enter_tree() -> void:
	_stopping = false

func _exit_tree() -> void:
	_stopping = true
	cancel()

func _allowed_url(url: String) -> bool:
	if not url.begins_with("https://"):
		return false
	var authority := url.trim_prefix("https://").get_slice("/", 0).to_lower()
	if authority.is_empty() or "@" in authority or ":" in authority or "\\" in url:
		return false
	return allowed_https_hosts.has(authority)

func resolve() -> Error:
	if _running or _stopping or rive_file == null or not is_inside_tree():
		return ERR_BUSY if _running else ERR_UNCONFIGURED
	_running = true
	_job += 1
	var job := _job
	var file := rive_file
	var original_data := file.data
	var original := file.asset_overrides.duplicate(true)
	var candidates := original.duplicate(true)
	var metadata := file.get_asset_metadata()
	if file.get_load_error_code() != OK:
		return _finish(job, file.get_load_error_code())
	for asset: Dictionary in metadata:
		if asset.get("embedded", false) or asset.get("overridden", false):
			continue
		var value: Variant = resource_resolver.call(asset.duplicate(true)) if resource_resolver.is_valid() else null
		if job != _job or not is_inside_tree(): return ERR_SKIP
		if value == null:
			if not url_resolver.is_valid(): return _finish(job, ERR_FILE_MISSING_DEPENDENCIES)
			var resolved_url: Variant = url_resolver.call(asset.duplicate(true))
			if job != _job or not is_inside_tree(): return ERR_SKIP
			if not resolved_url is String or not _allowed_url(resolved_url):
				return _finish(job, ERR_UNAUTHORIZED)
			_request = HTTPRequest.new()
			_request.max_redirects = 0
			_request.body_size_limit = max_download_megabytes * 1024 * 1024
			_request.timeout = timeout_seconds
			add_child(_request)
			_request.request_completed.connect(_on_response.bind(job), CONNECT_ONE_SHOT)
			var error := _request.request(resolved_url)
			if error != OK: return _finish(job, error)
			var response: Array = []
			while true:
				var packet: Array = await _response_received
				if packet[0] == job:
					response = packet[1]
					break
			if job != _job or not is_inside_tree(): return ERR_SKIP
			_request.queue_free()
			_request = null
			if response[0] != HTTPRequest.RESULT_SUCCESS or response[1] != 200:
				return _finish(job, ERR_CANT_CONNECT)
			value = response[3]
		if not (value is PackedByteArray or value is Texture2D or value is FontFile or value is String):
			return _finish(job, ERR_INVALID_DATA)
		candidates[asset.id] = value
		asset_resolved.emit(asset.duplicate(true))
		if job != _job or not is_inside_tree(): return ERR_SKIP
	if rive_file != file or file.data != original_data or file.asset_overrides != original:
		return _finish(job, ERR_BUSY)
	var candidate := RiveFile.new()
	candidate.data = original_data
	candidate.asset_overrides = candidates
	if candidate.asset_overrides != candidates: return _finish(job, ERR_INVALID_DATA)
	var decoded := candidate.load_rive_file()
	if decoded != OK: return _finish(job, decoded)
	file.asset_overrides = candidates
	if file.asset_overrides != candidates: return _finish(job, ERR_INVALID_DATA)
	return _finish(job, file.load_rive_file())

func _on_response(result: int, code: int, headers: PackedStringArray, body: PackedByteArray, job: int) -> void:
	if job == _job: _response_received.emit(job, [result, code, headers, body])

func _finish(job: int, error: Error) -> Error:
	if job != _job: return ERR_SKIP
	if is_instance_valid(_request): _request.queue_free()
	_request = null
	_running = false
	completed.emit(error)
	return error

func _process(delta: float) -> void:
	if not watch_dependencies or rive_file == null or _running: return
	_watch_clock += delta
	if _watch_clock < 1.0: return
	_watch_clock = 0.0
	var times := {}
	for path in rive_file.get_external_dependencies():
		times[path] = FileAccess.get_modified_time(path)
	var changed := not _dependency_times.is_empty() and _dependency_times != times
	_dependency_times = times
	if changed: rive_file.reload_assets()
