#define HW_RVL 1
#include "tracy/TracyC.h"
#include <gccore.h>
#include <malloc.h>
#include <math.h>
#include <network.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#include <ogc/usbgecko.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <wiiuse/wpad.h>

GXRModeObj*	 screenMode;
static void* frameBuffer;
static vu8	 readyForCopy;
#define FIFO_SIZE (256 * 1024)

s16 vertices[] ATTRIBUTE_ALIGN(32) = {
	-15, 15, 0, // TL
	15, 15, 0,	// TR
	15, -15, 0, // BR
	-15, -15, 0 // BL
};

u8 colors[] ATTRIBUTE_ALIGN(32) = {
	255, 0, 0, 255,	 // red (TL)
	0, 255, 0, 255,	 // green (TR)
	0, 0, 255, 255,	 // blue (BR)
	255, 255, 0, 255 // yellow (BL)
};

void		update_screen(Mtx viewMatrix, f32 angle);
static void copy_buffers(u32 unused);

static size_t	mem1_offset = 0;
static uint8_t* mem1_base;
static size_t	mem1_size;

void init_mem1_region()
{
	void* lo = SYS_GetArena1Lo();
	void* hi = (void*)((uintptr_t)SYS_GetArena1Hi() - 2 * 1024 * 1024);

	mem1_base = (uint8_t*)lo;
	mem1_size = (uintptr_t)hi - (uintptr_t)lo;

	// Mark Arena1 as fully consumed so nothing else touches it
	SYS_SetArena1Lo(hi);
}

static size_t	mem2_offset = 0;
static uint8_t* mem2_base;
static size_t	mem2_size;

void init_mem2_region()
{
	void* lo = SYS_GetArena2Lo();
	void* hi = (void*)((uintptr_t)SYS_GetArena2Hi() - 2 * 1024 * 1024);

	mem2_base = (uint8_t*)lo;
	mem2_size = (uintptr_t)hi - (uintptr_t)lo;

	// Mark Arena2 as fully consumed so nothing else touches it
	SYS_SetArena2Lo(hi);
}

int main(void)
{
	___tracy_startup_profiler();
	// init_mem1_region();
	init_mem2_region();
	Mtx		view;
	Mtx44	projection;
	GXColor backgroundColor = { 0, 0, 0, 255 };
	void*	fifoBuffer = NULL;

	u64 lastTime = 0;
	u64 currentTime = 0;
	f32 deltaTime = 0.f;
	f32 rotation = 0.f;
	f32 rotationSpeed = 90.f;

	VIDEO_Init();
	WPAD_Init();
	screenMode = VIDEO_GetPreferredMode(NULL);

	frameBuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(screenMode));

	VIDEO_Configure(screenMode);
	VIDEO_SetNextFramebuffer(frameBuffer);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	VIDEO_SetPostRetraceCallback(copy_buffers);

	fifoBuffer = MEM_K0_TO_K1(memalign(32, FIFO_SIZE));
	memset(fifoBuffer, 0, FIFO_SIZE);

	GX_Init(fifoBuffer, FIFO_SIZE);
	GX_SetCopyClear(backgroundColor, 0x00ffffff);
	GX_SetViewport(0, 0, screenMode->fbWidth, screenMode->efbHeight, 0, 1);
	GX_SetDispCopyYScale((f32)screenMode->xfbHeight / (f32)screenMode->efbHeight);
	GX_SetScissor(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopySrc(0, 0, screenMode->fbWidth, screenMode->efbHeight);
	GX_SetDispCopyDst(screenMode->fbWidth, screenMode->xfbHeight);
	GX_SetCopyFilter(screenMode->aa, screenMode->sample_pattern,
		GX_TRUE, screenMode->vfilter);
	GX_SetFieldMode(screenMode->field_rendering,
		((screenMode->viHeight == 2 * screenMode->xfbHeight) ? GX_ENABLE : GX_DISABLE));

	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer, GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	guVector camera = { 0.0F, 0.0F, 0.0F };
	guVector up = { 0.0F, 1.0F, 0.0F };
	guVector look = { 0.0F, 0.0F, -1.0F };

	guPerspective(projection, 60, (CONF_GetAspectRatio() == CONF_ASPECT_16_9) ? 16.0F / 9.0F : 4.0F / 3.0F, 10.0F, 300.0F);
	GX_LoadProjectionMtx(projection, GX_PERSPECTIVE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_INDEX8);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetArray(GX_VA_POS, vertices, 3 * sizeof(s16));
	GX_SetArray(GX_VA_CLR0, colors, 4 * sizeof(u8));
	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	lastTime = gettime();

	while (1)
	{
		TracyCZone(ctx, 1);

		currentTime = gettime();
		deltaTime = (float)ticks_to_nanosecs(currentTime - lastTime) / (float)TB_NSPERSEC;
		lastTime = currentTime;

		rotation += rotationSpeed * deltaTime;
		if (rotation >= 360.f)
		{
			rotation -= 360.f;
		}

		guLookAt(view, &camera, &up, &look);
		GX_SetViewport(0, 0, screenMode->fbWidth, screenMode->efbHeight, 0, 1);
		GX_InvVtxCache();
		GX_InvalidateTexAll();
		update_screen(view, rotation);

		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
		{
			return 0;
		}

		/*
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_RIGHT)
		{
			ZoneScopedN("Mem1 Alloc Input");

			void* testMem = mem1_base + mem1_offset;
			TracyAllocN(testMem, 10000, "MEM_1");
			memset(testMem, 0, 10000);
			mem1_offset += 10000;
		}
		*/

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_LEFT)
		{
			TracyCZone(b_ctx, 1);
			TracyCZoneName(b_ctx, "Mem2 Alloc Input", 16);

			void* testMem = mem2_base + mem2_offset;
			TracyCAllocN(testMem, 4 * 1024 * 1024, "MEM_2");
			memset(testMem, 0, 4 * 1024 * 1024);
			mem2_offset += 4 * 1024 * 1024;

			TracyCZoneEnd(b_ctx);
		}

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_B)
		{
			TracyCZone(b_ctx, 1);
			TracyCZoneName(b_ctx, "B button", 8);

			volatile double x = 0.0;
			for (int i = 0; i < 2000000; ++i)
			{
				x += sin(i * 0.001) * cos(i * 0.002);
			}

			TracyCZoneEnd(b_ctx);
		}

		TracyCZoneEnd(ctx);
		TracyCFrameMark;
	}
	___tracy_shutdown_profiler();
	return 0;
}

void update_screen(Mtx viewMatrix, f32 angle)
{
	TracyCZoneN(screen_ctx, "Screen Update", strlen("Screen Update"));
	Mtx modelView;
	Mtx rotation;

	guMtxIdentity(modelView);
	guMtxRotDeg(rotation, 'z', angle);
	guMtxConcat(rotation, modelView, modelView);
	guMtxTransApply(modelView, modelView, 0.0F, 0.0F, -50.0F);
	guMtxConcat(viewMatrix, modelView, modelView);

	GX_LoadPosMtxImm(modelView, GX_PNMTX0);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position1x8(0);
	GX_Color1x8(0);
	GX_Position1x8(1);
	GX_Color1x8(1);
	GX_Position1x8(2);
	GX_Color1x8(2);
	GX_Position1x8(3);
	GX_Color1x8(3);

	GX_End();

	GX_DrawDone();
	readyForCopy = GX_TRUE;

	VIDEO_WaitVSync();
	TracyCZoneEnd(screen_ctx);
	return;
}

static void copy_buffers(u32 count __attribute__((unused)))
{
	if (readyForCopy == GX_TRUE)
	{
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer, GX_TRUE);
		GX_Flush();
		readyForCopy = GX_FALSE;
	}
}
