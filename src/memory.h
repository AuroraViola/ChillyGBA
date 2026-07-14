#include "common.h"

struct Memory {
	u8 *bios;
	u8 *ewram;
	u8 *iwram;
	u8 *io;
	u8 *pram;
	u8 *vram;
	u8 *oam;
	u8 *gamepak;
};

static void mem_init(struct Memory *m) {
	m->bios = malloc(0x4000);
	m->ewram = malloc(0x40000);
	m->iwram = malloc(0x8000);
	m->io = malloc(0x400);
	m->pram = malloc(0x400);
	m->vram = malloc(0x18000);
	m->oam = malloc(0x400);
	m->gamepak = malloc(0x2000000);
}

static u8 mem_read_8(struct Memory *m, u32 addr) {
	switch (addr) {
		case 0x0 ... 0x4000:
			return m->bios[addr];
		case 0x02000000 ... 0x0203FFFF:
			return m->ewram[addr - 0x02000000];
		case 0x03000000 ... 0x03007FFF:
			return m->iwram[addr - 0x03000000];
		case 0x04000000 ... 0x040003FF:
			return m->io[addr - 0x04000000];
		case 0x05000000 ... 0x050003FF:
			return m->pram[addr - 0x05000000];
		case 0x06000000 ... 0x06017FFF:
			return m->vram[addr - 0x06000000];
		case 0x07000000 ... 0x070003FF:
			return m->oam[addr - 0x07000000];
		case 0x08000000 ... 0x09FFFFFF:
			return m->gamepak[addr - 0x08000000];
		default:
			return 0xff;
	}
}
