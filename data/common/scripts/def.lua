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
		error(_msg, _stackLevel)
	end
end


function StringHash(_str)
	return FrmCore.StringHash(_str)
end
