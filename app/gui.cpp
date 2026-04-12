#include "gui.hpp"

#include <srat/core-types.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "raylib.h"

#include <cmath>
#include <cstdio>

// ai generated for image viewer tool

// ─────────────────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────────────────
enum class ViewMode : u8 {
	SideBySide = 0,
	Slider,
	DiffOnly,
};

// ─────────────────────────────────────────────────────────
// DiffStats
// ─────────────────────────────────────────────────────────
struct DiffStats {
	f32 maxErr;   // 0–255
	f32 meanErr;
	f32 psnr;     // extremely large when identical
	u32 diffPx;
	u32 totalPx;
};

// ─────────────────────────────────────────────────────────
// Persistent GUI state
// ─────────────────────────────────────────────────────────
struct GuiState {
	void const * lastDataPtr;
	bool         initialized;

	// GPU uploads for ImGui::Image display.
	// Managed internally; rebuilt when input images change.
	Texture2D gpuRefColor;
	Texture2D gpuRefDepth;
	Texture2D gpuDevColor;
	Texture2D gpuDevDepth;
	Texture2D gpuDiffColor;
	Texture2D gpuDiffDepth;

	// Histogram-filtered GPU textures
	Texture2D gpuHistRefColor;
	Texture2D gpuHistRefDepth;
	Texture2D gpuHistDevColor;
	Texture2D gpuHistDevDepth;
	bool  showHistogram;
	f32   histMin;
	f32   histMax;


	DiffStats colorStats;
	DiffStats depthStats;

	ViewMode viewMode;
	i32      channel;    // 0 = Color, 1 = Depth

	f32 zoom;
	f32 panX;
	f32 panY;

	f32  sliderT;        // [0, 1] divider position
	bool draggingSlider;

	f32  diffAmplify;
	bool showInspector;
};

// ─────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────
namespace {

static DiffStats computeStats(
	Image const & ref,
	Image const & dev
) {
	u8 const * rp = (u8 const *)ref.data;
	u8 const * dp = (u8 const *)dev.data;
	i32 const  n  = ref.width * ref.height;

	f64 sumSq  = 0.0;
	f64 sumAbs = 0.0;
	f32 maxErr = 0.0f;
	u32 diffPx = 0;

	for (i32 i = 0; i < n; i++) {
		i32 const b    = i * 4;
		bool      diff = false;
		for (i32 c = 0; c < 3; c++) {
			f32 const d  = (f32)((i32)rp[b+c] - (i32)dp[b+c]);
			f32 const ad = d < 0.0f ? -d : d;
			sumAbs += (f64)ad;
			sumSq  += (f64)(d * d);
			if (ad > maxErr) maxErr = ad;
			if (ad > 0.0f)   diff   = true;
		}
		if (diff) diffPx++;
	}

	f64 const total = (f64)(n * 3);
	f32 const mean  = (f32)(sumAbs / total);
	f64 const mse   = sumSq / total;
	f32 const psnr  = (f32)(
		mse < 1e-12
		? 1.0e9
		: 10.0 * std::log10(255.0 * 255.0 / mse)
	);

	return DiffStats {
		.maxErr  = maxErr,
		.meanErr = mean,
		.psnr    = psnr,
		.diffPx  = diffPx,
		.totalPx = (u32)n,
	};
}

static void buildDiffTex(
	Image const & ref,
	Image const & dev,
	f32 amp,
	Texture2D & tex
) {
	i32 const n   = ref.width * ref.height;
	Image     tmp = GenImageColor(ref.width, ref.height, BLACK);
	u8 *       out = (u8 *)tmp.data;
	u8 const * rp  = (u8 const *)ref.data;
	u8 const * dp  = (u8 const *)dev.data;

	for (i32 i = 0; i < n; i++) {
		i32 const b = i * 4;
		for (i32 c = 0; c < 3; c++) {
			i32 const d = (i32)rp[b+c] - (i32)dp[b+c];
			i32 const a = (i32)((d < 0 ? -d : d) * amp + 0.5f);
			out[b+c] = (u8)(a > 255 ? 255 : a);
		}
		out[b+3] = 255;
	}

	if (tex.id == 0)
		tex = LoadTextureFromImage(tmp);
	else
		UpdateTexture(tex, tmp.data);

	UnloadImage(tmp);
}

// Compute per-pixel luminance (0–1) from an RGBA8 image.
static inline f32 pixelLuminance(u8 const * p) {
	return (0.299f * (f32)p[0] + 0.587f * (f32)p[1] + 0.114f * (f32)p[2])
		/ 255.0f;
}

// Build a brightness-remapped texture: pixels with luminance
// outside [lo, hi] go black; pixels inside get rescaled so
// the visible slice fills the full 0–255 range.
static void buildHistTex(
	Image const & src,
	f32 lo, f32 hi,
	Texture2D & tex
) {
	i32 const n   = src.width * src.height;
	Image     tmp = GenImageColor(src.width, src.height, BLACK);
	u8 *       out = (u8 *)tmp.data;
	u8 const * sp  = (u8 const *)src.data;
	f32 const range = (hi - lo) > 1e-6f ? (hi - lo) : 1e-6f;

	for (i32 i = 0; i < n; i++) {
		i32 const b = i * 4;
		f32 const L = pixelLuminance(sp + b);
		if (L < lo || L > hi) {
			out[b+0] = 0; out[b+1] = 0; out[b+2] = 0; out[b+3] = 255;
		} else {
			f32 const t = (L - lo) / range;
			for (i32 c = 0; c < 3; c++) {
				f32 const v = (f32)sp[b+c] * t;
				out[b+c] = (u8)(v > 255.f ? 255 : (v + 0.5f));
			}
			out[b+3] = 255;
		}
	}

	if (tex.id == 0)
		tex = LoadTextureFromImage(tmp);
	else
		UpdateTexture(tex, tmp.data);

	UnloadImage(tmp);
}

// Draw a 256-bin luminance histogram with the selected range
// highlighted, using the window DrawList.
static void drawHistogram(
	Image const & img,
	f32 lo, f32 hi,
	f32 width, f32 height
) {
	// this doesn't work
	// // Accumulate bins
	// u32 bins[256] = {};
	// u8 const * p = (u8 const *)img.data;
	// i32 const  n = img.width * img.height;
	// u32 maxBin = 1;
	// for (i32 i = 0; i < n; i++) {
	// 	u8 const L = (u8)(pixelLuminance(p + i*4) * 255.0f + 0.5f);
	// 	bins[L]++;
	// }
	// for (i32 i = 0; i < 256; i++)
	// 	if (bins[i] > maxBin) maxBin = bins[i];

	// // Draw via DrawList
	// ImVec2 const cursor = ImGui::GetCursorScreenPos();
	// ImDrawList * dl = ImGui::GetWindowDrawList();
	// f32 const barW = width / 256.0f;

	// i32 const loB = (i32)(lo * 255.0f + 0.5f);
	// i32 const hiB = (i32)(hi * 255.0f + 0.5f);

	// for (i32 i = 0; i < 256; i++) {
	// 	f32 const h = ((f32)bins[i] / (f32)maxBin) * height;
	// 	f32 const x = cursor.x + (f32)i * barW;
	// 	bool const inRange = (i >= loB && i <= hiB);
	// 	ImU32 const col = inRange
	// 		? IM_COL32(100, 180, 255, 220)
	// 		: IM_COL32(60, 60, 70, 160);
	// 	dl->AddRectFilled(
	// 		ImVec2(x, cursor.y + height - h),
	// 		ImVec2(x + barW, cursor.y + height),
	// 		col
	// 	);
	// }

	// // Reserve the space in ImGui layout
	// ImGui::Dummy(ImVec2(width, height));
}


// Draw a Texture2D into a DrawList rect with zoom/pan and
// scissor clipping. All panels share identical zoom/pan so
// they scroll in lock-step.
static void drawTexInRect(
	ImDrawList * dl,
	ImVec2 pMin, ImVec2 pMax,
	Texture2D const & tex,
	f32 zoom, f32 panX, f32 panY
) {
	f32 const pw = pMax.x - pMin.x;
	f32 const ph = pMax.y - pMin.y;
	f32 const tw = (f32)tex.width  * zoom;
	f32 const th = (f32)tex.height * zoom;
	f32 const ix = pMin.x + (pw - tw) * 0.5f + panX;
	f32 const iy = pMin.y + (ph - th) * 0.5f + panY;

	dl->PushClipRect(pMin, pMax, true);
	dl->AddImage(
		(ImTextureID)(uintptr_t)tex.id,
		ImVec2(ix, iy),
		ImVec2(ix + tw, iy + th)
	);
	dl->PopClipRect();
}

// Map a screen position to texel coordinates.
// Returns false when outside the texture bounds or panel.
static bool toTexel(
	ImVec2 mouse,
	ImVec2 pMin, ImVec2 pMax,
	i32 texW, i32 texH,
	f32 zoom, f32 panX, f32 panY,
	i32 & outX, i32 & outY
) {
	f32 const pw = pMax.x - pMin.x;
	f32 const ph = pMax.y - pMin.y;
	f32 const tw = (f32)texW * zoom;
	f32 const th = (f32)texH * zoom;
	f32 const ox = pMin.x + (pw - tw) * 0.5f + panX;
	f32 const oy = pMin.y + (ph - th) * 0.5f + panY;

	outX = (i32)((mouse.x - ox) / zoom);
	outY = (i32)((mouse.y - oy) / zoom);

	return (
		outX >= 0 && outX < texW &&
		outY >= 0 && outY < texH &&
		mouse.x >= pMin.x && mouse.x < pMax.x &&
		mouse.y >= pMin.y && mouse.y < pMax.y
	);
}

// Direct CPU pixel read from an RGBA8 Raylib Image.
static Color samplePixel(Image const & img, i32 x, i32 y) {
	u8 const * p = (u8 const *)img.data;
	i32 const  i = (y * img.width + x) * 4;
	return Color{ p[i], p[i+1], p[i+2], p[i+3] };
}

// Small overlay badge in the top-left corner of a panel.
static void panelBadge(
	ImDrawList * dl,
	ImVec2 pMin,
	char const * text,
	ImU32 textCol
) {
	ImVec2 const tSz = ImGui::CalcTextSize(text);
	ImVec2 const bMin = ImVec2(pMin.x + 4.0f, pMin.y + 4.0f);
	ImVec2 const bMax = ImVec2(
		bMin.x + tSz.x + 10.0f,
		bMin.y + tSz.y + 8.0f
	);
	dl->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 180));
	dl->AddRect(bMin, bMax, IM_COL32(80, 80, 100, 180));
	dl->AddText(
		ImVec2(bMin.x + 5.0f, bMin.y + 4.0f),
		textCol, text
	);
}

// Pixel inspector tooltip.
// Reads pixel values directly from the CPU Image data.
static void showInspector(
	ImVec2 pMin, ImVec2 pMax,
	Image const & refImg,
	Image const & devImg,
	f32 zoom, f32 panX, f32 panY
) {
	ImVec2 const mouse = ImGui::GetIO().MousePos;
	i32 tx = 0, ty = 0;
	bool const hit = toTexel(
		mouse, pMin, pMax,
		refImg.width, refImg.height,
		zoom, panX, panY, tx, ty
	);
	if (!hit) return;

	Color const rc = samplePixel(refImg, tx, ty);
	Color const dc = samplePixel(devImg, tx, ty);
	i32 const dr  = (i32)rc.r - (i32)dc.r;
	i32 const dg  = (i32)rc.g - (i32)dc.g;
	i32 const db  = (i32)rc.b - (i32)dc.b;
	i32 const da  = (i32)rc.a - (i32)dc.a;
	bool const eq = (dr == 0 && dg == 0 && db == 0);

	static constexpr ImVec4 kGood = { 0.30f, 0.85f, 0.40f, 1.0f };
	static constexpr ImVec4 kBad  = { 0.90f, 0.30f, 0.30f, 1.0f };
	static constexpr ImVec4 kDim  = { 0.60f, 0.60f, 0.65f, 1.0f };

	ImGui::BeginTooltip();

	ImGui::TextColored(
		ImVec4(0.40f, 0.70f, 1.0f, 1.0f),
		"px (%d, %d)", tx, ty
	);

	ImGui::Separator();

	ImGui::Text(
		"Ref  R:%-3d G:%-3d B:%-3d A:%-3d",
		rc.r, rc.g, rc.b, rc.a
	);
	ImGui::Text(
		"Dev  R:%-3d G:%-3d B:%-3d A:%-3d",
		dc.r, dc.g, dc.b, dc.a
	);

	ImVec4 const diffCol = eq ? kGood : kBad;
	ImGui::TextColored(
		diffCol,
		"Dif  R:%+d G:%+d B:%+d A:%+d",
		dr, dg, db, da
	);

	ImGui::Separator();

	// Color swatches
	ImGui::ColorButton(
		"##rsw",
		ImVec4(rc.r/255.f, rc.g/255.f, rc.b/255.f, 1.f),
		ImGuiColorEditFlags_NoTooltip |
		ImGuiColorEditFlags_NoBorder,
		ImVec2(20.f, 20.f)
	);
	ImGui::SameLine();
	ImGui::TextColored(kDim, "Ref");
	ImGui::SameLine(0.f, 20.f);
	ImGui::ColorButton(
		"##dsw",
		ImVec4(dc.r/255.f, dc.g/255.f, dc.b/255.f, 1.f),
		ImGuiColorEditFlags_NoTooltip |
		ImGuiColorEditFlags_NoBorder,
		ImVec2(20.f, 20.f)
	);
	ImGui::SameLine();
	ImGui::TextColored(kDim, "Dev");

	ImGui::Separator();
	ImGui::TextColored(eq ? kGood : kBad,
		eq ? "MATCH" : "MISMATCH");

	ImGui::EndTooltip();
}

// Stats bar shown at the bottom of the window.
static void drawStatsBar(
	DiffStats const & st,
	char const * chName
) {
	static constexpr ImVec4 kGood = { 0.30f, 0.85f, 0.40f, 1.0f };
	static constexpr ImVec4 kWarn = { 0.90f, 0.72f, 0.20f, 1.0f };
	static constexpr ImVec4 kBad  = { 0.90f, 0.30f, 0.30f, 1.0f };
	static constexpr ImVec4 kDim  = { 0.55f, 0.55f, 0.60f, 1.0f };

	ImGui::Separator();

	// Title row: channel name + right-aligned PASS/FAIL
	bool const pass      = (st.maxErr < 1.0f);
	char const * badge   = pass ? "[ PASS ]" : "[ FAIL ]";
	ImVec4 const badgeCol = pass ? kGood : kBad;
	ImVec2 const badgeSz = ImGui::CalcTextSize(badge);

	ImGui::Text("%s Stats", chName);
	ImGui::SameLine(
		ImGui::GetCursorPosX()
		+ ImGui::GetContentRegionAvail().x
		- badgeSz.x
	);
	ImGui::TextColored(badgeCol, "%s", badge);

	// Four stat columns
	ImGui::Columns(4, "##stats", false);

	// Max Error
	ImGui::TextColored(kDim, "Max Error");
	ImVec4 const maxCol = (
		st.maxErr < 1.0f  ? kGood :
		st.maxErr < 10.0f ? kWarn : kBad
	);
	ImGui::TextColored(maxCol, "%.2f", st.maxErr);
	ImGui::NextColumn();

	// Mean Error
	ImGui::TextColored(kDim, "Mean Error");
	ImGui::Text("%.5f", st.meanErr);
	ImGui::NextColumn();

	// PSNR
	ImGui::TextColored(kDim, "PSNR");
	ImVec4 const psnrCol = (
		st.psnr > 999.f ? kGood :
		st.psnr > 60.f  ? kGood :
		st.psnr > 40.f  ? kWarn : kBad
	);
	if (st.psnr > 999.f)
		ImGui::TextColored(psnrCol, "INF dB");
	else
		ImGui::TextColored(psnrCol, "%.1f dB", st.psnr);
	ImGui::NextColumn();

	// Differing pixels
	ImGui::TextColored(kDim, "Differing Pixels");
	f32 const pct = (
		st.totalPx > 0
		? 100.0f * (f32)st.diffPx / (f32)st.totalPx
		: 0.0f
	);
	ImVec4 const dpCol = (
		st.diffPx == 0 ? kGood :
		st.diffPx < 5  ? kWarn : kBad
	);
	ImGui::TextColored(dpCol,
		"%u / %u  (%.2f%%)",
		st.diffPx, st.totalPx, pct
	);
	ImGui::Columns(1);

	ImGui::TextDisabled(
		"[1/2/3] View   [C/D] Channel   "
		"[Scroll/+/-] Zoom   [RMB/MMB] Pan   "
		"[R] Reset   [A] Amp   [I] Inspector"
		"[H] Histogram"
	);
}

} // namespace

// ─────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────
void guiUnitTestImages(
	Image const & referenceColor,
	Image const & referenceDepth,
	Image const & deviceColor,
	Image const & deviceDepth
) {
	static GuiState s = {};

	// ── Init / re-init when image data pointer changes ───
	bool const needInit = (
		!s.initialized ||
		s.lastDataPtr != referenceColor.data
	);

	if (needInit) {
		if (s.initialized) {
			UnloadTexture(s.gpuRefColor);
			UnloadTexture(s.gpuRefDepth);
			UnloadTexture(s.gpuDevColor);
			UnloadTexture(s.gpuDevDepth);
			UnloadTexture(s.gpuDiffColor);
			UnloadTexture(s.gpuDiffDepth);
			if (s.gpuHistRefColor.id) { UnloadTexture(s.gpuHistRefColor); }
			if (s.gpuHistRefDepth.id) { UnloadTexture(s.gpuHistRefDepth); }
			if (s.gpuHistDevColor.id) { UnloadTexture(s.gpuHistDevColor); }
			if (s.gpuHistDevDepth.id) { UnloadTexture(s.gpuHistDevDepth); }
		}

		// Upload reference + device to GPU for display
		s.gpuRefColor = LoadTextureFromImage(referenceColor);
		s.gpuRefDepth = LoadTextureFromImage(referenceDepth);
		s.gpuDevColor = LoadTextureFromImage(deviceColor);
		s.gpuDevDepth = LoadTextureFromImage(deviceDepth);

		// Build amplified diff textures from CPU data
		f32 const amp = s.diffAmplify > 0.f ? s.diffAmplify : 8.f;
		s.gpuDiffColor = {};
		s.gpuDiffDepth = {};
		buildDiffTex(
			referenceColor, deviceColor, amp, s.gpuDiffColor
		);
		buildDiffTex(
			referenceDepth, deviceDepth, amp, s.gpuDiffDepth
		);

		// Compute stats from CPU data directly
		s.colorStats = computeStats(referenceColor, deviceColor);
		s.depthStats = computeStats(referenceDepth, deviceDepth);

		if (!s.initialized) {
			s.viewMode       = ViewMode::SideBySide;
			s.channel        = 0;
			s.zoom           = 1.0f;
			s.panX           = 0.0f;
			s.panY           = 0.0f;
			s.sliderT        = 0.5f;
			s.draggingSlider = false;
			s.diffAmplify    = 8.0f;
			s.showInspector  = true;
			s.showHistogram  = false;
			s.histMin  = 0.0f;
			s.histMax  = 1.0f;
		}

		s.gpuHistRefColor = {};
		s.gpuHistRefDepth = {};
		s.gpuHistDevColor = {};
		s.gpuHistDevDepth = {};

		s.lastDataPtr = referenceColor.data;
		s.initialized = true;
	}

	// ── ImGui window ─────────────────────────────────────
	ImGui::SetNextWindowSize(
		ImVec2(1100.f, 760.f), ImGuiCond_FirstUseEver
	);
	ImGuiWindowFlags const wFlags = (
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse
	);
	if (!ImGui::Begin("Unit Test Viewer", nullptr, wFlags)) {
		ImGui::End();
		return;
	}

	// ── Keyboard shortcuts (window must be focused) ──────
	if (ImGui::IsWindowFocused(
		ImGuiFocusedFlags_RootAndChildWindows
	)) {
		if (ImGui::IsKeyPressed(ImGuiKey_1))
			s.viewMode = ViewMode::SideBySide;
		if (ImGui::IsKeyPressed(ImGuiKey_2))
			s.viewMode = ViewMode::Slider;
		if (ImGui::IsKeyPressed(ImGuiKey_3))
			s.viewMode = ViewMode::DiffOnly;
		if (ImGui::IsKeyPressed(ImGuiKey_C)) s.channel = 0;
		if (ImGui::IsKeyPressed(ImGuiKey_D)) s.channel = 1;
		if (ImGui::IsKeyPressed(ImGuiKey_I))
			s.showInspector = !s.showInspector;
		if (ImGui::IsKeyPressed(ImGuiKey_H))
			s.showHistogram = !s.showHistogram;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) {
			s.zoom = 1.0f;
			s.panX = 0.0f;
			s.panY = 0.0f;
		}

		// Zoom with + / -
		if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
		    ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
			f32 const nz = s.zoom * 1.25f;
			s.zoom = nz > 64.f ? 64.f : nz;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
		    ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
			f32 const nz = s.zoom * 0.8f;
			s.zoom = nz < 0.1f ? 0.1f : nz;
		}

		// Cycle diff amplification: 1x → 2x → 4x → 8x → 16x
		if (ImGui::IsKeyPressed(ImGuiKey_A)) {
			static constexpr f32 kAmps[] = {
				1.f, 2.f, 4.f, 8.f, 16.f
			};
			i32 cur = 0;
			for (i32 i = 0; i < 5; i++) {
				if (std::abs(s.diffAmplify - kAmps[i]) < 0.1f) {
					cur = i;
					break;
				}
			}
			s.diffAmplify = kAmps[(cur + 1) % 5];
			buildDiffTex(
				referenceColor, deviceColor,
				s.diffAmplify, s.gpuDiffColor
			);
			buildDiffTex(
				referenceDepth, deviceDepth,
				s.diffAmplify, s.gpuDiffDepth
			);
		}
	}

	// ── Select active texture set based on channel ───────
	bool const isColor = (s.channel == 0);

	Texture2D const & refTex = isColor
		? s.gpuRefColor : s.gpuRefDepth;
	Texture2D const & devTex = isColor
		? s.gpuDevColor : s.gpuDevDepth;
	Texture2D const & difTex = isColor
		? s.gpuDiffColor : s.gpuDiffDepth;
	Image const & refImg = isColor
		? referenceColor : referenceDepth;
	Image const & devImg = isColor
		? deviceColor : deviceDepth;
	DiffStats const & stats = isColor
		? s.colorStats : s.depthStats;
	char const * chName = isColor ? "Color" : "Depth";

	// When histogram is active, swap ref/dev textures
	// for the filtered versions
	Texture2D const & dispRefTex = (
		s.showHistogram && (isColor ? s.gpuHistRefColor.id : s.gpuHistRefDepth.id)
		? (isColor ? s.gpuHistRefColor : s.gpuHistRefDepth)
		: refTex
	);
	Texture2D const & dispDevTex = (
		s.showHistogram && (isColor ? s.gpuHistDevColor.id : s.gpuHistDevDepth.id)
		? (isColor ? s.gpuHistDevColor : s.gpuHistDevDepth)
		: devTex
	);


	// ── Toolbar ──────────────────────────────────────────
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);

	// View mode buttons
	auto viewButton = [&](char const * label, ViewMode mode) {
		bool const active = (s.viewMode == mode);
		if (active) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(0.22f, 0.47f, 0.85f, 1.f)
			);
		}
		if (ImGui::Button(label)) s.viewMode = mode;
		if (active) ImGui::PopStyleColor();
		ImGui::SameLine();
	};

	// Histogram toggle
	{
		bool const on = s.showHistogram;
		if (on) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(0.22f, 0.47f, 0.85f, 1.f)
			);
		}
		if (ImGui::Button("Histogram")) {
			s.showHistogram = !s.showHistogram;
		}
		if (on) ImGui::PopStyleColor();
		ImGui::SameLine();
	}


	viewButton("Side-by-Side", ViewMode::SideBySide);
	viewButton("Slider",       ViewMode::Slider);
	viewButton("Diff Only",    ViewMode::DiffOnly);

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// Channel buttons (annotated with OK/error indicator)
	auto chButton = [&](char const * label, i32 ch) {
		DiffStats const & chSt = (ch == 0)
			? s.colorStats : s.depthStats;
		bool const active = (s.channel == ch);
		bool const clean  = (chSt.maxErr < 1.f);
		if (active) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(0.22f, 0.47f, 0.85f, 1.f)
			);
		}
		char buf[32];
		std::snprintf(buf, sizeof(buf),
			"%s %s", label, clean ? "(OK)" : "(!)");
		if (!clean && !active) {
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				ImVec4(0.9f, 0.4f, 0.4f, 1.f)
			);
		}
		if (ImGui::Button(buf)) s.channel = ch;
		if (!clean && !active) ImGui::PopStyleColor();
		if (active)             ImGui::PopStyleColor();
		ImGui::SameLine();
	};

	chButton("Color", 0);
	chButton("Depth", 1);

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// Inspector toggle
	{
		bool const on = s.showInspector;
		if (on) {
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(0.22f, 0.47f, 0.85f, 1.f)
			);
		}
		if (ImGui::Button("Inspector")) {
			s.showInspector = !s.showInspector;
		}
		if (on) ImGui::PopStyleColor();
		ImGui::SameLine();
	}

	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// Zoom / amp read-outs
	ImGui::TextDisabled("Zoom %.2fx", s.zoom);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::TextDisabled("Amp %.0fx", s.diffAmplify);

	ImGui::PopStyleVar();

	// ── Histogram panel (when active) ─────────────────────
	if (s.showHistogram) {
		f32 const prevMin = s.histMin;
		f32 const prevMax = s.histMax;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
		ImGui::Separator();
		ImGui::Text("Brightness Range");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(300.f);
		ImGui::DragFloatRange2(
			"##histRange",
			&s.histMin, &s.histMax,
			0.002f, 0.0f, 1.0f,
			"Min: %.3f", "Max: %.3f"
		);
		if (s.histMin > s.histMax) s.histMin = s.histMax;

		// Draw the histogram
		f32 const histW = ImGui::GetContentRegionAvail().x;
		drawHistogram(refImg, s.histMin, s.histMax, histW, 60.f);
		ImGui::PopStyleVar();

		// Rebuild filtered textures when range changes
		bool const rangeChanged = (
			s.histMin != prevMin || s.histMax != prevMax
			|| (!isColor ? !s.gpuHistRefDepth.id : !s.gpuHistRefColor.id)
		);
		if (rangeChanged) {
			buildHistTex(refImg, s.histMin, s.histMax,
				isColor ? s.gpuHistRefColor : s.gpuHistRefDepth);
			buildHistTex(devImg, s.histMin, s.histMax,
				isColor ? s.gpuHistDevColor : s.gpuHistDevDepth);
		}
	}


	// ── Image area sizing ─────────────────────────────────
	// Reserve space for stats bar at the bottom.
	static constexpr f32 kStatsH = 96.f;
	f32 const imgH = (
		ImGui::GetContentRegionAvail().y - kStatsH
	);
	f32 const imgW = ImGui::GetContentRegionAvail().x;

	// ── Image panels (InvisibleButton + DrawList) ─────────
	// We reserve rects via InvisibleButton first, then draw
	// into all of them via the window DrawList.
	ImDrawList * dl = ImGui::GetWindowDrawList();

	// These are filled in per-mode; refPanel is used for
	// pixel-inspector coordinate mapping.
	ImVec2 refPanMin = {}, refPanMax = {};

	bool anyHov = false;
	bool anyAct = false;

	if (s.viewMode == ViewMode::SideBySide) {
		static constexpr f32 kGap = 4.f;
		f32 const pw = (imgW - kGap * 2.f) / 3.f;
		ImVec2 const pSz = ImVec2(pw, imgH);

		// Reserve panel rects
		ImGui::InvisibleButton("##ref", pSz);
		ImVec2 const r0Min = ImGui::GetItemRectMin();
		ImVec2 const r0Max = ImGui::GetItemRectMax();
		bool const h0 = ImGui::IsItemHovered();
		bool const a0 = ImGui::IsItemActive();

		ImGui::SameLine(0.f, kGap);
		ImGui::InvisibleButton("##dev", pSz);
		ImVec2 const r1Min = ImGui::GetItemRectMin();
		ImVec2 const r1Max = ImGui::GetItemRectMax();
		bool const h1 = ImGui::IsItemHovered();
		bool const a1 = ImGui::IsItemActive();

		ImGui::SameLine(0.f, kGap);
		ImGui::InvisibleButton("##dif", pSz);
		ImVec2 const r2Min = ImGui::GetItemRectMin();
		ImVec2 const r2Max = ImGui::GetItemRectMax();
		bool const h2 = ImGui::IsItemHovered();
		bool const a2 = ImGui::IsItemActive();

		anyHov = h0 || h1 || h2;
		anyAct = a0 || a1 || a2;

		// Backgrounds
		dl->AddRectFilled(
			r0Min, r0Max, IM_COL32(18, 18, 22, 255)
		);
		dl->AddRectFilled(
			r1Min, r1Max, IM_COL32(18, 18, 22, 255)
		);
		dl->AddRectFilled(
			r2Min, r2Max, IM_COL32(18, 18, 22, 255)
		);

		// Textures
		drawTexInRect(dl, r0Min, r0Max,
			dispRefTex, s.zoom, s.panX, s.panY);
		drawTexInRect(dl, r1Min, r1Max,
			dispDevTex, s.zoom, s.panX, s.panY);
		drawTexInRect(dl, r2Min, r2Max,
			difTex, s.zoom, s.panX, s.panY);

		// Panel borders
		dl->AddRect(r0Min, r0Max, IM_COL32(55, 55, 72, 180));
		dl->AddRect(r1Min, r1Max, IM_COL32(55, 55, 72, 180));
		dl->AddRect(r2Min, r2Max, IM_COL32(55, 55, 72, 180));

		// Labels
		panelBadge(dl, r0Min, "Reference",
			IM_COL32(72, 210, 90, 255));
		panelBadge(dl, r1Min, "Device",
			IM_COL32(220, 175, 55, 255));
		panelBadge(dl, r2Min, "Diff (amplified)",
			IM_COL32(220, 72, 72, 255));

		// Inspector maps from whichever panel is hovered
		refPanMin = h1 ? r1Min : (h2 ? r2Min : r0Min);
		refPanMax = h1 ? r1Max : (h2 ? r2Max : r0Max);

	} else if (s.viewMode == ViewMode::Slider) {
		ImGui::InvisibleButton("##sldr", ImVec2(imgW, imgH));
		ImVec2 const sMin = ImGui::GetItemRectMin();
		ImVec2 const sMax = ImGui::GetItemRectMax();
		anyHov = ImGui::IsItemHovered();
		anyAct = ImGui::IsItemActive();

		// Background
		dl->AddRectFilled(
			sMin, sMax, IM_COL32(18, 18, 22, 255)
		);

		// Reference fills the full panel
		drawTexInRect(dl, sMin, sMax,
			dispRefTex, s.zoom, s.panX, s.panY);

		// Device clipped to the right of the divider
		f32 const divX = sMin.x + (sMax.x - sMin.x) * s.sliderT;
		{
			f32 const pw = sMax.x - sMin.x;
			f32 const ph = sMax.y - sMin.y;
			f32 const tw = (f32)dispDevTex.width  * s.zoom;
			f32 const th = (f32)dispDevTex.height * s.zoom;
			f32 const ix = sMin.x + (pw - tw) * 0.5f + s.panX;
			f32 const iy = sMin.y + (ph - th) * 0.5f + s.panY;
			dl->PushClipRect(
				ImVec2(divX, sMin.y), sMax, true
			);
			dl->AddImage(
				(ImTextureID)(uintptr_t)dispDevTex.id,
				ImVec2(ix, iy),
				ImVec2(ix + tw, iy + th)
			);
			dl->PopClipRect();
		}

		// Divider line + handle
		bool const nearDiv = (
			std::abs(ImGui::GetIO().MousePos.x - divX) < 12.f
			&& anyHov
		);
		ImU32 const divCol = (nearDiv || s.draggingSlider)
			? IM_COL32(255, 220, 60, 240)
			: IM_COL32(255, 220, 60, 160);
		f32 const midY = sMin.y + (sMax.y - sMin.y) * 0.5f;

		dl->AddLine(
			ImVec2(divX, sMin.y), ImVec2(divX, sMax.y),
			divCol, 2.f
		);
		dl->AddCircleFilled(
			ImVec2(divX, midY), 10.f, divCol
		);
		dl->AddCircle(
			ImVec2(divX, midY), 10.f,
			IM_COL32(255, 255, 255, 120), 0, 1.5f
		);

		// Side labels
		{
			char const * lbl = "Reference";
			ImVec2 const lSz = ImGui::CalcTextSize(lbl);
			f32 const leftX = divX - lSz.x - 18.f;
			if (leftX > sMin.x + 4.f) {
				dl->AddRectFilled(
					ImVec2(leftX - 4.f, midY - 14.f),
					ImVec2(divX - 8.f,  midY + 4.f),
					IM_COL32(0, 0, 0, 160)
				);
				dl->AddText(
					ImVec2(leftX, midY - 12.f),
					IM_COL32(72, 210, 90, 230), lbl
				);
			}
		}
		{
			char const * lbl = "Device";
			f32 const rightX = divX + 18.f;
			ImVec2 const lSz = ImGui::CalcTextSize(lbl);
			if (rightX + lSz.x + 4.f < sMax.x) {
				dl->AddRectFilled(
					ImVec2(divX + 8.f,  midY - 14.f),
					ImVec2(rightX + lSz.x + 4.f, midY + 4.f),
					IM_COL32(0, 0, 0, 160)
				);
				dl->AddText(
					ImVec2(rightX, midY - 12.f),
					IM_COL32(220, 175, 55, 230), lbl
				);
			}
		}

		// Divider drag with LMB
		if (anyHov && nearDiv &&
		    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			s.draggingSlider = true;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			s.draggingSlider = false;
		}
		if (s.draggingSlider) {
			f32 const pw = sMax.x - sMin.x;
			f32 const raw = (
				(ImGui::GetIO().MousePos.x - sMin.x) / pw
			);
			s.sliderT = raw < 0.f ? 0.f : (raw > 1.f ? 1.f : raw);
		}

		// Panel border
		dl->AddRect(sMin, sMax, IM_COL32(55, 55, 72, 180));

		refPanMin = sMin;
		refPanMax = sMax;

	} else { // DiffOnly
		ImGui::InvisibleButton(
			"##donly", ImVec2(imgW, imgH)
		);
		ImVec2 const dMin = ImGui::GetItemRectMin();
		ImVec2 const dMax = ImGui::GetItemRectMax();
		anyHov = ImGui::IsItemHovered();
		anyAct = ImGui::IsItemActive();

		dl->AddRectFilled(
			dMin, dMax, IM_COL32(18, 18, 22, 255)
		);
		drawTexInRect(dl, dMin, dMax,
			difTex, s.zoom, s.panX, s.panY);
		dl->AddRect(dMin, dMax, IM_COL32(55, 55, 72, 180));

		panelBadge(dl, dMin, "Diff (amplified)",
			IM_COL32(220, 72, 72, 255));

		refPanMin = dMin;
		refPanMax = dMax;
	}

	// ── Zoom with scroll wheel ────────────────────────────
	if (anyHov) {
		f32 const wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.f) {
			f32 const nz = s.zoom * std::pow(1.15f, wheel);
			s.zoom = nz < 0.1f ? 0.1f : (nz > 64.f ? 64.f : nz);
		}
	}

	// ── Pan with RMB or MMB ───────────────────────────────
	// Explicitly exclude LMB so slider drag doesn't conflict.
	bool const rmbDown = ImGui::IsMouseDown(
		ImGuiMouseButton_Right
	);
	bool const mmbDown = ImGui::IsMouseDown(
		ImGuiMouseButton_Middle
	);
	if ((anyHov || anyAct) && (rmbDown || mmbDown)) {
		ImVec2 const md = ImGui::GetIO().MouseDelta;
		s.panX += md.x;
		s.panY += md.y;
	}

	// ── Pixel inspector tooltip ───────────────────────────
	if (s.showInspector && anyHov) {
		showInspector(
			refPanMin, refPanMax,
			refImg, devImg,
			s.zoom, s.panX, s.panY
		);
	}

	// ── Stats bar ─────────────────────────────────────────
	drawStatsBar(stats, chName);

	ImGui::End();
}
