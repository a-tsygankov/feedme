import { useEffect, useState } from "react";
import { ApiError, type Cat, api, auth } from "../lib/api";
import { CAT_PALETTE, resolveCatColor, toCssHex } from "../lib/palette";

// Cats page. Shows the home's cats with rename + recolor edit;
// also lets the user add (lowest free slot) and remove (soft delete,
// refused for the last cat). Mirrors the firmware CatsList +
// CatEditView UX so the two sides feel consistent.
export default function CatsPage() {
  const [cats, setCats] = useState<Cat[] | null>(null);
  const [err, setErr] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  // editing.slotId === undefined → no edit modal; -1 = add new.
  const [editing, setEditing] = useState<Cat | { slotId: -1; name: ""; color: 0; slug: "C2"; defaultPortionG: 40; hungryThresholdSec: 18000 } | null>(null);

  function bounceOn401(e: unknown) {
    if (e instanceof ApiError && e.status === 401) {
      auth.clear();
      location.replace("/login");
      return true;
    }
    return false;
  }

  async function reload() {
    try {
      const { cats } = await api.catsList();
      setCats(cats);
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "load failed");
    }
  }

  useEffect(() => { void reload(); }, []);

  async function saveEdit(patch: Partial<Cat>) {
    if (!editing) return;
    setBusy(true); setErr(null);
    try {
      if (editing.slotId === -1) {
        await api.catsCreate(patch);
      } else {
        await api.catsUpdate(editing.slotId, patch);
      }
      setEditing(null);
      await reload();
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "save failed");
    } finally {
      setBusy(false);
    }
  }

  async function removeCat(slotId: number) {
    if (!confirm("Remove this cat? Past feeds keep their attribution.")) return;
    setBusy(true); setErr(null);
    try {
      await api.catsDelete(slotId);
      await reload();
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "remove failed");
    } finally {
      setBusy(false);
    }
  }

  const canAdd    = (cats?.length ?? 0) < 4;
  const canRemove = (cats?.length ?? 0) >= 2;

  return (
    <>
      <h1>Cats</h1>
      {err && <p className="error">{err}</p>}
      <div className="card">
        {!cats && <p className="muted">Loading…</p>}
        {cats?.map(c => (
          <div key={c.slotId} className="row">
            <span className="swatch" style={{ background: toCssHex(resolveCatColor(c.slotId, c.color)) }} />
            <span style={{ flex: 1 }}>
              {c.name}
              <div className="muted" style={{ fontSize: 12 }}>
                {c.defaultPortionG} g · {c.slug}
              </div>
            </span>
            <button className="secondary" onClick={() => setEditing(c)}>Edit</button>
          </div>
        ))}
        {canAdd && (
          <div className="row">
            <button className="secondary" style={{ width: "100%" }}
                    onClick={() => setEditing({
                      slotId: -1, name: "", color: 0, slug: "C2",
                      defaultPortionG: 40, hungryThresholdSec: 18000,
                    })}>
              + Add cat
            </button>
          </div>
        )}
      </div>

      {editing && (
        <CatEditModal
          initial={editing}
          busy={busy}
          canRemove={canRemove && editing.slotId !== -1}
          onCancel={() => setEditing(null)}
          onSave={saveEdit}
          onRemove={editing.slotId !== -1 ? () => removeCat(editing.slotId) : undefined}
        />
      )}
    </>
  );
}

// ── Modal: rename + recolor + portion. Slug picker is a follow-up. ─
interface ModalProps {
  initial: Cat | { slotId: -1; name: ""; color: 0; slug: "C2"; defaultPortionG: 40; hungryThresholdSec: 18000 };
  busy: boolean;
  canRemove: boolean;
  onCancel: () => void;
  onSave: (patch: Partial<Cat>) => void;
  onRemove?: () => void;
}

function CatEditModal({ initial, busy, canRemove, onCancel, onSave, onRemove }: ModalProps) {
  const [name, setName] = useState(initial.name);
  const [color, setColor] = useState(initial.color);
  const [portion, setPortion] = useState(initial.defaultPortionG);

  const isNew = initial.slotId === -1;

  return (
    <div className="card">
      <h2>{isNew ? "New cat" : `Edit ${initial.name}`}</h2>

      <label>Name</label>
      <input
        autoFocus
        value={name}
        maxLength={15}
        onChange={(e) => setName(e.target.value)}
        placeholder={isNew ? "e.g. Mochi" : initial.name}
      />

      <label>Colour</label>
      <div style={{ display: "flex", gap: 10, flexWrap: "wrap" }}>
        {CAT_PALETTE.map(p => (
          <button
            key={p.rgb}
            onClick={() => setColor(p.rgb)}
            title={p.name}
            style={{
              background: toCssHex(p.rgb),
              width: 36, height: 36, borderRadius: 18, padding: 0,
              border: color === p.rgb ? "3px solid var(--ink)" : "1px solid var(--line)",
              boxShadow: color === p.rgb ? "0 0 0 2px var(--bg) inset" : "none",
              cursor: "pointer",
            }}
          />
        ))}
      </div>

      <label>Default portion (grams)</label>
      <input
        type="number"
        min={5} max={200}
        value={portion}
        onChange={(e) => setPortion(Number(e.target.value))}
      />

      <div style={{ display: "flex", gap: 10, marginTop: 20 }}>
        <button className="secondary" onClick={onCancel} style={{ flex: 1 }}>Cancel</button>
        <button
          disabled={busy || name.trim().length === 0}
          onClick={() => onSave({
            name: name.trim(),
            color: color || undefined,   // 0 = "let backend pick" — auto-colour
            defaultPortionG: portion,
          })}
          style={{ flex: 1 }}
        >
          {busy ? "..." : isNew ? "Add" : "Save"}
        </button>
      </div>

      {onRemove && (
        <button
          className="danger"
          disabled={!canRemove || busy}
          onClick={onRemove}
          style={{ width: "100%", marginTop: 12 }}
          title={canRemove ? undefined : "Can't remove the last cat"}
        >
          {canRemove ? "Remove cat" : "Can't remove last cat"}
        </button>
      )}
    </div>
  );
}
