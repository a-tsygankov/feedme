import { Navigate, NavLink, Route, Routes } from "react-router-dom";
import { auth } from "./lib/api";
import CatsPage from "./views/CatsPage";
import HomePage from "./views/HomePage";
import LoginPage from "./views/LoginPage";
import SettingsPage from "./views/SettingsPage";
import UsersPage from "./views/UsersPage";

// Top-level chrome. Auth gating is route-side: if the user has no
// token, every authed page Navigate's them to /login. The bottom tab
// bar shows only when a token is present (keeps /login clean).
export default function App() {
  const isAuthed = !!auth.token();
  return (
    <div className="app">
      <Routes>
        <Route path="/login" element={<LoginPage />} />
        <Route path="/"           element={isAuthed ? <HomePage />    : <Navigate to="/login" replace />} />
        <Route path="/cats"       element={isAuthed ? <CatsPage />    : <Navigate to="/login" replace />} />
        <Route path="/users"      element={isAuthed ? <UsersPage />   : <Navigate to="/login" replace />} />
        <Route path="/settings"   element={isAuthed ? <SettingsPage />: <Navigate to="/login" replace />} />
        <Route path="*"           element={<Navigate to="/" replace />} />
      </Routes>
      {isAuthed && (
        <nav className="tabs">
          <NavLink to="/"         end className={({ isActive }) => isActive ? "active" : ""}>Home</NavLink>
          <NavLink to="/cats"         className={({ isActive }) => isActive ? "active" : ""}>Cats</NavLink>
          <NavLink to="/users"        className={({ isActive }) => isActive ? "active" : ""}>Users</NavLink>
          <NavLink to="/settings"     className={({ isActive }) => isActive ? "active" : ""}>Settings</NavLink>
        </nav>
      )}
    </div>
  );
}
