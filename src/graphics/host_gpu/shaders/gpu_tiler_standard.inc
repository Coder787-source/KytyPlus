uint standard_offset(uvec2 p) {
	uint x = p.x, y = p.y;
	switch (ELEMENT_BYTES) {
	case 1: return ((y << 4) & 0x1f0u) ^ ((y << 5) & 0x400u) ^ (x & 0x00fu) ^
	               ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
	case 2: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^
	               ((x << 1) & 0x00eu) ^ ((x << 4) & 0x080u) ^
	               ((x << 5) & 0x200u) ^ ((x << 6) & 0x800u);
	case 4: return ((y << 4) & 0x070u) ^ ((y << 5) & 0x100u) ^ ((y << 6) & 0x400u) ^
	               ((x << 2) & 0x00cu) ^ ((x << 5) & 0x080u) ^
	               ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
	case 8: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^
	               ((x << 3) & 0x008u) ^ ((x << 5) & 0x0c0u) ^
	               ((x << 6) & 0x200u) ^ ((x << 7) & 0x800u);
	default: return ((y << 4) & 0x030u) ^ ((y << 6) & 0x100u) ^ ((y << 7) & 0x400u) ^
	                ((x << 6) & 0x0c0u) ^ ((x << 7) & 0x200u) ^ ((x << 8) & 0x800u);
	}
}
