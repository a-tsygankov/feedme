// Cat face — Simon's-Cat-style SVG character matched to the mockup
// (cat-feeder-mockups.jsx). 5 moods: happy / neutral / warning /
// hungry / fed / sleepy. The "warning" mood reuses the neutral face
// — it's the ring color that conveys urgency at that step; the cat
// only changes expression when the user can actually do something
// about it (Hungry → please feed me; Fed → satisfied).
//
// Geometry is the same 0..120 viewbox as the mockup so paths can
// be lifted verbatim. Stroke + fill colours come from a small palette
// that defaults to the original Simon's Cat off-white + black.

import type { ReactElement } from "react";
import type { Mood } from "../lib/mood";

const SW = 2.6;
const W  = "#f4f4ef";   // catWhite
const BL = "#1c1c1c";   // catLine
const PK = "#ffb3c1";   // catPink

// Body parts shared across the calm moods (head + body + paws).
function Body() {
  return (
    <>
      <path d="M76 100 C96 88 100 70 88 64" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={60} cy={97} rx={19} ry={14} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={49} cy={109} rx={9} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={71} cy={109} rx={9} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
    </>
  );
}

function Head() {
  return (
    <>
      <polygon points="29,27 24,8 46,22"  fill={W}  stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="32,25 28,12 43,22" fill={PK} stroke="none"/>
      <polygon points="91,27 96,8 74,22"  fill={W}  stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="88,25 92,12 77,22" fill={PK} stroke="none"/>
      <circle cx={60} cy={50} r={33} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* whiskers */}
      <line x1={27} y1={50} x2={46} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={27} y1={56} x2={46} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={27} y1={62} x2={46} y2={60} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={93} y1={50} x2={74} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={93} y1={56} x2={74} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={93} y1={62} x2={74} y2={60} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <path d="M57,57 L60,61 L63,57 Z" fill={PK} stroke={BL} strokeWidth={1.2} strokeLinejoin="round"/>
    </>
  );
}

function Happy() {
  return (
    <g>
      <Body/><Head/>
      <ellipse cx={47} cy={44} rx={10} ry={11} fill={BL}/>
      <ellipse cx={73} cy={44} rx={10} ry={11} fill={BL}/>
      <ellipse cx={51} cy={40} rx={3.8} ry={4.2} fill={W}/>
      <ellipse cx={77} cy={40} rx={3.8} ry={4.2} fill={W}/>
      <circle cx={44} cy={49} r={1.5} fill={W} opacity={0.7}/>
      <circle cx={70} cy={49} r={1.5} fill={W} opacity={0.7}/>
      <path d="M48,64 Q60,76 72,64" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={38} cy={57} rx={6} ry={3.5} fill={PK} opacity={0.45}/>
      <ellipse cx={82} cy={57} rx={6} ry={3.5} fill={PK} opacity={0.45}/>
    </g>
  );
}

function Neutral() {
  return (
    <g>
      <Body/><Head/>
      <ellipse cx={47} cy={46} rx={10} ry={11} fill={BL}/>
      <ellipse cx={73} cy={46} rx={10} ry={11} fill={BL}/>
      <ellipse cx={47} cy={39} rx={11.5} ry={8} fill={W}/>
      <ellipse cx={73} cy={39} rx={11.5} ry={8} fill={W}/>
      <path d="M37,46 Q47,38 57,46" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <path d="M63,46 Q73,38 83,46" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={51} cy={47} rx={3} ry={3} fill={W}/>
      <ellipse cx={77} cy={47} rx={3} ry={3} fill={W}/>
      <line x1={52} y1={67} x2={68} y2={67} stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
    </g>
  );
}

function Hungry() {
  return (
    <g>
      {/* compact sitting body w/ raised paw */}
      <path d="M74 104 C90 96 96 80 84 74" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={60} cy={98} rx={17} ry={13} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={71} cy={110} rx={9} ry={5} fill={W} stroke={BL} strokeWidth={SW}/>
      {/* raised arm */}
      <path d="M48 88 C40 80 34 70 33 60" fill="none" stroke={BL} strokeWidth={SW + 0.4} strokeLinecap="round"/>
      <path d="M33 60 C32 52 36 46 42 42" fill="none" stroke={BL} strokeWidth={SW + 0.4} strokeLinecap="round"/>
      <ellipse cx={45} cy={39} rx={9} ry={7.5} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={45} cy={29} rx={4} ry={5.5} fill={W} stroke={BL} strokeWidth={SW}/>
      <ellipse cx={38} cy={35} rx={3.5} ry={3} fill={W} stroke={BL} strokeWidth={1.8}/>
      <ellipse cx={52} cy={35} rx={3.5} ry={3} fill={W} stroke={BL} strokeWidth={1.8}/>
      {/* head shifted right */}
      <polygon points="51,24 47,8 64,20"  fill={W}  stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="53,22 50,11 62,20" fill={PK} stroke="none"/>
      <polygon points="86,24 91,8 74,20"  fill={W}  stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <polygon points="84,22 88,11 76,20" fill={PK} stroke="none"/>
      <circle cx={68} cy={50} r={31} fill={W} stroke={BL} strokeWidth={SW}/>
      <line x1={99} y1={48} x2={82} y2={50} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={99} y1={54} x2={82} y2={54} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={99} y1={60} x2={82} y2={58} stroke={BL} strokeWidth={1.4} strokeLinecap="round"/>
      <line x1={37} y1={50} x2={54} y2={52} stroke={BL} strokeWidth={1.4} strokeLinecap="round" opacity={0.7}/>
      <line x1={37} y1={56} x2={54} y2={56} stroke={BL} strokeWidth={1.4} strokeLinecap="round" opacity={0.7}/>
      <path d="M65,58 L68,62 L71,58 Z" fill={PK} stroke={BL} strokeWidth={1.2} strokeLinejoin="round"/>
      {/* eyebrows */}
      <path d="M52,30 Q60,24 68,29" fill="none" stroke={BL} strokeWidth={2.4} strokeLinecap="round"/>
      <path d="M68,29 Q76,24 84,29" fill="none" stroke={BL} strokeWidth={2.4} strokeLinecap="round"/>
      {/* eyes + tears */}
      <ellipse cx={58} cy={43} rx={11} ry={12} fill={BL}/>
      <ellipse cx={79} cy={43} rx={11} ry={12} fill={BL}/>
      <ellipse cx={63} cy={37} rx={5} ry={5.5} fill={W}/>
      <ellipse cx={84} cy={37} rx={5} ry={5.5} fill={W}/>
      <circle cx={54} cy={48} r={2} fill={W} opacity={0.7}/>
      <circle cx={75} cy={48} r={2} fill={W} opacity={0.7}/>
      <path d="M54,55 Q53,60 55,62" fill="none" stroke="#93c5fd" strokeWidth={1.8} strokeLinecap="round" opacity={0.8}/>
      <path d="M75,55 Q74,60 76,62" fill="none" stroke="#93c5fd" strokeWidth={1.8} strokeLinecap="round" opacity={0.8}/>
      {/* mouth */}
      <path d="M56,66 Q58,63 62,63 Q66,63 68,66 L67,74 Q64,79 60,79 Q56,79 53,74 Z"
            fill={BL} stroke={BL} strokeWidth={1.5} strokeLinejoin="round"/>
      <path d="M57,67 Q59,65 62,65 Q65,65 67,67 L66,73 Q63,77 60,77 Q57,77 54,73 Z"
            fill="#ff6b8a" stroke="none"/>
      <ellipse cx={61} cy={74} rx={4} ry={3} fill="#ff8fa3" stroke="none"/>
      <rect x={59} y={64} width={4} height={3} rx={1.5} fill={W}/>
    </g>
  );
}

function Fed() {
  return (
    <g>
      <Body/><Head/>
      <path d="M38,45 Q47,36 57,45" fill="none" stroke={BL} strokeWidth={3} strokeLinecap="round"/>
      <path d="M63,45 Q72,36 82,45" fill="none" stroke={BL} strokeWidth={3} strokeLinecap="round"/>
      <path d="M47,63 Q60,78 73,63" fill={BL} stroke={BL} strokeWidth={SW} strokeLinejoin="round"/>
      <path d="M47,63 Q60,72 73,63" fill={W} stroke="none"/>
      <ellipse cx={38} cy={58} rx={7} ry={4} fill={PK} opacity={0.55}/>
      <ellipse cx={82} cy={58} rx={7} ry={4} fill={PK} opacity={0.55}/>
      <text x={85} y={26} fontSize={15} fill="#f87171" style={{ userSelect: "none" }}>♥</text>
    </g>
  );
}

function Sleepy() {
  return (
    <g>
      <Body/><Head/>
      <ellipse cx={47} cy={48} rx={10} ry={9} fill={BL}/>
      <ellipse cx={73} cy={48} rx={10} ry={9} fill={BL}/>
      <ellipse cx={47} cy={41} rx={11.5} ry={8} fill={W}/>
      <ellipse cx={73} cy={41} rx={11.5} ry={8} fill={W}/>
      <path d="M37,48 Q47,41 57,48" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <path d="M63,48 Q73,41 83,48" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <ellipse cx={51} cy={49} rx={2.5} ry={2.5} fill={W}/>
      <ellipse cx={77} cy={49} rx={2.5} ry={2.5} fill={W}/>
      <path d="M54,66 Q60,70 66,66" fill="none" stroke={BL} strokeWidth={SW} strokeLinecap="round"/>
      <text x={78} y={30} fontSize={10} fill="#6b6b80" fontWeight="700" fontFamily="sans-serif">z</text>
      <text x={84} y={21} fontSize={13} fill="#6b6b80" fontWeight="700" fontFamily="sans-serif">z</text>
      <text x={91} y={11} fontSize={16} fill="#6b6b80" fontWeight="700" fontFamily="sans-serif">Z</text>
    </g>
  );
}

const FACES: Record<Mood, () => ReactElement> = {
  happy:   Happy,
  neutral: Neutral,
  warning: Neutral,   // same expression, ring color carries the urgency
  hungry:  Hungry,
  fed:     Fed,
  sleepy:  Sleepy,
};

interface Props {
  mood: Mood;
  size?: number;
}

export default function CatFace({ mood, size = 100 }: Props) {
  const Face = FACES[mood] ?? Happy;
  return (
    <svg
      viewBox="0 0 120 118"
      width={size}
      height={size * 118 / 120}
      style={{ display: "block", overflow: "visible" }}
    >
      <Face/>
    </svg>
  );
}
