// Mood calculation — same shape as the firmware's MoodCalculator.
// Maps "seconds since last fed" against the cat's hungry threshold
// to a mood enum + ring color. Ratios match the device so a cat that
// looks "Warning" on the LCD also looks "Warning" in the webapp.
//
//   ratio = secondsSince / hungryThresholdSec
//   ratio < 0.5            → Happy   (green)
//   0.5 ≤ ratio < 0.75     → Neutral (yellow)
//   0.75 ≤ ratio < 1.0     → Warning (orange)
//   1.0 ≤ ratio            → Hungry  (red, pulsing in UI)
//
// Special cases:
//   - secondsSince == null  → Neutral (no events yet — first run)
//   - lastEventType=="snooze" doesn't reset the timer; the ratio
//     keeps climbing. Snooze just dampens the urgency presentation
//     elsewhere (e.g. switching the label from "FEED ME!" to
//     "Just begging").

export type Mood = "happy" | "neutral" | "warning" | "hungry" | "fed" | "sleepy";

export interface MoodInfo {
  mood:  Mood;
  color: string;        // CSS hex
  label: string;        // short status string for under the ring
}

const COLORS = {
  green:  "#4ade80",
  yellow: "#facc15",
  orange: "#fb923c",
  red:    "#f87171",
  blue:   "#818cf8",
} as const;

export function moodFor(
  secondsSince: number | null,
  thresholdSec: number,
): MoodInfo {
  if (secondsSince === null || thresholdSec <= 0) {
    return { mood: "neutral", color: COLORS.yellow, label: "no data" };
  }
  const r = secondsSince / thresholdSec;
  if (r < 0.5)  return { mood: "happy",   color: COLORS.green,  label: "Recently fed" };
  if (r < 0.75) return { mood: "neutral", color: COLORS.yellow, label: "Getting peckish" };
  if (r < 1.0)  return { mood: "warning", color: COLORS.orange, label: "Feed soon" };
  return         { mood: "hungry",  color: COLORS.red,    label: "Overdue!" };
}

// Ring fill 0..1 — full at "just fed", drains to 0 at threshold.
// Capped at 0 so post-threshold the ring stays empty (the color +
// pulsing animation carry the urgency past that point).
export function ringProgress(
  secondsSince: number | null,
  thresholdSec: number,
): number {
  if (secondsSince === null || thresholdSec <= 0) return 0;
  return Math.max(0, 1 - secondsSince / thresholdSec);
}

// "5m ago" / "2h 15m ago" / "—" formatter, identical phrasing to the
// mockup's formatAgo helper. seconds=null returns the em-dash so
// the UI stays compact even when no events exist yet.
export function formatAgo(secondsSince: number | null): string {
  if (secondsSince === null) return "—";
  const m = Math.floor(secondsSince / 60);
  if (m < 1)  return "just now";
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60), r = m % 60;
  return r ? `${h}h ${r}m ago` : `${h}h ago`;
}
