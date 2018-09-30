require "FrmCore"

function LOG(_msg)
	FrmCore.Log(_msg)
end
function LOG_DBG(_msg)
	FrmCore.LogDbg(_msg)
end
function LOG_ERR(_msg)
	FrmCore.LogErr(_msg)
end

function ASSERT(_v, _msg, _stackLevel)
	if (not _v) then
		_msg = _msg or ""
		_stackLevel = _stackLevel or 2
		error(_msg, _stackLevel + 1)
	end
	return _v
end

function StringHash(_str)
	return FrmCore.StringHash(_str)
end

function StackInfo(_stackLevel)
	local info = ASSERT(debug.getinfo(_stackLevel, "Sln"), "", _stackLevel + 1)
	if (info.source == "") then
		return string.format("%s, line %d:", info.name, info.currentline)
	else
		return string.format("%s, file '%s', line %d:", info.name, info.source, info.currentline)
	end
end

function CopyRecursive(_x)
	if (type(_x) ~= 'table') then
		return _x
	end
	local ret = setmetatable({}, getmetatable(_x))
	for k, v in pairs(_x) do
		ret[CopyRecursive(k)] = CopyRecursive(v)
	end
	return ret
end
