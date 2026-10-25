"""Space Cab: what the shared CHGame tools need to know (the schema is
chgame/tools/gamecfg.py's docstring)."""

# One framebuffer row a DMA chunk (CHGfx's default is 2): 512 B of RAM back.
DEFINES = ["GFX_CHUNK_ROWS=1"]
