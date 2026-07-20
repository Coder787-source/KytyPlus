uint standard3d_offset(uvec3 p) {
	uint x = p.x, y = p.y, z = p.z;
	if (ELEMENT_BYTES == 1)
		return (x & 3u) ^ ((x << 4) & 0x40u) ^ ((x << 6) & 0x200u) ^
		       ((y << 3) & 8u) ^ ((y << 4) & 0x20u) ^ ((y << 6) & 0x100u) ^
		       ((y << 8) & 0x800u) ^ ((z << 2) & 4u) ^ ((z << 3) & 0x10u) ^
		       ((z << 5) & 0x80u) ^ ((z << 7) & 0x400u);
	if (ELEMENT_BYTES == 2)
		return ((x << 1) & 2u) ^ ((x << 5) & 0x40u) ^ ((x << 7) & 0x200u) ^
		       ((y << 3) & 8u) ^ ((y << 4) & 0x20u) ^ ((y << 6) & 0x100u) ^
		       ((y << 8) & 0x800u) ^ ((z << 2) & 4u) ^ ((z << 3) & 0x10u) ^
		       ((z << 5) & 0x80u) ^ ((z << 7) & 0x400u);
	uint z_part = ((z << 4) & 0x10u) ^ ((z << 6) & 0x80u) ^ ((z << 8) & 0x400u);
	if (ELEMENT_BYTES == 4)
		return ((x << 2) & 4u) ^ ((x << 5) & 0x40u) ^ ((x << 7) & 0x200u) ^
		       ((y << 3) & 8u) ^ ((y << 4) & 0x20u) ^ ((y << 6) & 0x100u) ^
		       ((y << 8) & 0x800u) ^ z_part;
	uint y_part = ((y << 5) & 0x20u) ^ ((y << 7) & 0x100u) ^ ((y << 9) & 0x800u);
	if (ELEMENT_BYTES == 8)
		return ((x << 3) & 8u) ^ ((x << 5) & 0x40u) ^ ((x << 7) & 0x200u) ^ y_part ^ z_part;
	return ((x << 6) & 0x40u) ^ ((x << 8) & 0x200u) ^ y_part ^ z_part;
}
