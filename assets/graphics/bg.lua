local process = require("wf.api.v1.process")
local superfamiconv = require("wf.api.v1.process.tools.superfamiconv")

process.emit_symbol("gfx_bg", superfamiconv.convert_tilemap(
	"bg.png",
	superfamiconv.config()
		:mode("wsc")
        :bpp(4)
        :no_remap()
))