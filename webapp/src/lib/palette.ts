// Avatar colour palettes — mirror what the firmware ships in
// domain/Palette.h. The web app shows the same colour wheel so picks
// here match what users see on the device. These are temporary bold
// colours (per the firmware comment "while we work through cat
// scenarios"); revert in lockstep with the firmware swap.

export interface PaletteColor {
  name: string;
  rgb:  number;   // 0xRRGGBB
}

export const CAT_PALETTE: PaletteColor[] = [
  { name: "Green",   rgb: 0x4CAF50 },
  { name: "Blue",    rgb: 0x2196F3 },
  { name: "Magenta", rgb: 0xE91E63 },
  { name: "Brown",   rgb: 0x8B4513 },
];

export const USER_PALETTE: PaletteColor[] = [
  { name: "Princeton Orange",  rgb: 0xFF8200 },
  { name: "Emerald",           rgb: 0x50C878 },
  { name: "Turquoise",         rgb: 0x40E0D0 },
  { name: "Medium Slate Blue", rgb: 0x7B68EE },
];

// Round-robin auto-color when slot has no explicit choice (matches
// the firmware's autoUserColor / per-cat palette[id % size] logic).
export const autoCatColor  = (slotId: number) => CAT_PALETTE [slotId % CAT_PALETTE .length].rgb;
export const autoUserColor = (slotId: number) => USER_PALETTE[slotId % USER_PALETTE.length].rgb;

// Coerce 0 ("server says auto") into the round-robin pick so the UI
// always renders a real colour swatch.
export const resolveCatColor  = (slotId: number, color: number) => color === 0 ? autoCatColor (slotId) : color;
export const resolveUserColor = (slotId: number, color: number) => color === 0 ? autoUserColor(slotId) : color;

export const toCssHex = (rgb: number): string =>
  `#${rgb.toString(16).padStart(6, "0")}`;
