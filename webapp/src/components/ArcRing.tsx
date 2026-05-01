// Mood ring — progress arc that drains as the cat gets hungrier.
// Visual port of the mockup's ArcRing component (cat-feeder-mockups.jsx)
// adapted for the webapp's layout (centred behind the cat face).
//
// Props:
//   - progress  0..1 from full (just fed) to empty (overdue)
//   - color     CSS color for the active arc
//   - size      pixel diameter
//   - sw        stroke width
//
// The track (background ring) is drawn with low opacity so the empty
// portion is still visible; the active arc gets a soft glow filter.
// Both transition with `transition` so a fresh /dashboard/cats poll
// animates rather than snapping.

interface Props {
  progress: number;
  color:    string;
  size?:    number;
  sw?:      number;
  pulse?:   boolean;   // gentle pulse for "Hungry" state
}

export default function ArcRing({ progress, color, size = 200, sw = 9, pulse = false }: Props) {
  const r = (size - sw) / 2;
  const cx = size / 2, cy = size / 2;
  const circ = 2 * Math.PI * r;
  return (
    <svg
      width={size}
      height={size}
      style={{
        position: "absolute",
        top: 0, left: 0,
        animation: pulse ? "feedme-ring-pulse 1.4s ease-in-out infinite" : undefined,
      }}
    >
      <circle
        cx={cx} cy={cy} r={r}
        fill="none"
        stroke="rgba(255,255,255,0.08)"
        strokeWidth={sw}
      />
      <circle
        cx={cx} cy={cy} r={r}
        fill="none"
        stroke={color}
        strokeWidth={sw}
        strokeLinecap="round"
        strokeDasharray={circ}
        strokeDashoffset={circ * (1 - Math.max(0, Math.min(1, progress)))}
        transform={`rotate(-90 ${cx} ${cy})`}
        style={{
          transition: "stroke-dashoffset 0.8s ease, stroke 0.8s ease",
          filter: `drop-shadow(0 0 6px ${color})`,
        }}
      />
    </svg>
  );
}
