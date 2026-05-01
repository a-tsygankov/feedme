import { useEffect, useState } from "react";
import { ApiError, type User, api, auth } from "../lib/api";
import { USER_PALETTE, resolveUserColor, toCssHex } from "../lib/palette";

// Users page — same shape as CatsPage but without portion / threshold
// fields. Mirrors the firmware UsersList + UserRemove flow.
export default function UsersPage() {
  const [users, setUsers] = useState<User[] | null>(null);
  const [err, setErr] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [editing, setEditing] = useState<User | { slotId: -1; name: ""; color: 0 } | null>(null);

  function bounceOn401(e: unknown) {
    if (e instanceof ApiError && e.status === 401) {
      auth.clear(); location.replace("/login");
      return true;
    }
    return false;
  }

  async function reload() {
    try {
      const { users } = await api.usersList();
      setUsers(users);
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "load failed");
    }
  }

  useEffect(() => { void reload(); }, []);

  async function saveEdit(patch: Partial<User>) {
    if (!editing) return;
    setBusy(true); setErr(null);
    try {
      if (editing.slotId === -1) await api.usersCreate(patch);
      else                       await api.usersUpdate(editing.slotId, patch);
      setEditing(null);
      await reload();
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "save failed");
    } finally { setBusy(false); }
  }

  async function removeUser(slotId: number) {
    if (!confirm("Remove this user? Past feeds keep their attribution.")) return;
    setBusy(true); setErr(null);
    try {
      await api.usersDelete(slotId);
      await reload();
    } catch (e) {
      if (bounceOn401(e)) return;
      setErr(e instanceof Error ? e.message : "remove failed");
    } finally { setBusy(false); }
  }

  const canAdd    = (users?.length ?? 0) < 4;
  const canRemove = (users?.length ?? 0) >= 2;

  return (
    <>
      <h1>Users</h1>
      {err && <p className="error">{err}</p>}
      <div className="card">
        {!users && <p className="muted">Loading…</p>}
        {users?.map(u => (
          <div key={u.slotId} className="row">
            <span className="swatch" style={{ background: toCssHex(resolveUserColor(u.slotId, u.color)) }} />
            <span style={{ flex: 1 }}>{u.name}</span>
            <button className="secondary" onClick={() => setEditing(u)}>Edit</button>
          </div>
        ))}
        {canAdd && (
          <div className="row">
            <button className="secondary" style={{ width: "100%" }}
                    onClick={() => setEditing({ slotId: -1, name: "", color: 0 })}>
              + Add user
            </button>
          </div>
        )}
      </div>

      {editing && (
        <UserEditModal
          initial={editing}
          busy={busy}
          canRemove={canRemove && editing.slotId !== -1}
          onCancel={() => setEditing(null)}
          onSave={saveEdit}
          onRemove={editing.slotId !== -1 ? () => removeUser(editing.slotId) : undefined}
        />
      )}
    </>
  );
}

interface ModalProps {
  initial: User | { slotId: -1; name: ""; color: 0 };
  busy: boolean;
  canRemove: boolean;
  onCancel: () => void;
  onSave: (patch: Partial<User>) => void;
  onRemove?: () => void;
}

function UserEditModal({ initial, busy, canRemove, onCancel, onSave, onRemove }: ModalProps) {
  const [name, setName] = useState(initial.name);
  const [color, setColor] = useState(initial.color);
  const isNew = initial.slotId === -1;

  return (
    <div className="card">
      <h2>{isNew ? "New user" : `Edit ${initial.name}`}</h2>
      <label>Name</label>
      <input
        autoFocus
        value={name}
        maxLength={15}
        onChange={(e) => setName(e.target.value)}
        placeholder={isNew ? "e.g. Andrey" : initial.name}
      />
      <label>Colour</label>
      <div style={{ display: "flex", gap: 10, flexWrap: "wrap" }}>
        {USER_PALETTE.map(p => (
          <button
            key={p.rgb}
            onClick={() => setColor(p.rgb)}
            title={p.name}
            style={{
              background: toCssHex(p.rgb),
              width: 36, height: 36, borderRadius: 18, padding: 0,
              border: color === p.rgb ? "3px solid var(--ink)" : "1px solid var(--line)",
              cursor: "pointer",
            }}
          />
        ))}
      </div>

      <div style={{ display: "flex", gap: 10, marginTop: 20 }}>
        <button className="secondary" onClick={onCancel} style={{ flex: 1 }}>Cancel</button>
        <button
          disabled={busy || name.trim().length === 0}
          onClick={() => onSave({ name: name.trim(), color: color || undefined })}
          style={{ flex: 1 }}
        >
          {busy ? "..." : isNew ? "Add" : "Save"}
        </button>
      </div>
      {onRemove && (
        <button className="danger" disabled={!canRemove || busy} onClick={onRemove}
                style={{ width: "100%", marginTop: 12 }}
                title={canRemove ? undefined : "Can't remove the last user"}>
          {canRemove ? "Remove user" : "Can't remove last user"}
        </button>
      )}
    </div>
  );
}
