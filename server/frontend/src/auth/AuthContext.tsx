import { createContext, useContext, useEffect, useMemo, useState, type ReactNode } from 'react';
import { api, setAccessToken } from '../api/client';
import type { LoginResponse } from '../api/types';

interface AuthState {
  user: LoginResponse | null;
  login: (user: LoginResponse) => void;
  updateSession: (user: LoginResponse) => void;
  logout: () => void;
}

const AuthContext = createContext<AuthState | null>(null);

function persistUser(u: LoginResponse) {
  if (u.token) {
    setAccessToken(u.token);
  }
  localStorage.setItem('flm_user', JSON.stringify(u));
}

function clearPersistedSession() {
  setAccessToken(null);
  localStorage.removeItem('flm_user');
  localStorage.removeItem('flm_token');
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<LoginResponse | null>(() => {
    const raw = localStorage.getItem('flm_user');
    const token = localStorage.getItem('flm_token');
    if (token) setAccessToken(token);
    return raw ? JSON.parse(raw) : null;
  });

  // Re-validate stored session so a wiped/reseeded server cannot show ghost localStorage data
  useEffect(() => {
    const token = localStorage.getItem('flm_token');
    if (!token) return;
    let cancelled = false;
    api.me()
      .then((fresh) => {
        if (cancelled) return;
        const merged = { ...fresh, token: fresh.token || token };
        persistUser(merged);
        setUser(merged);
      })
      .catch(() => {
        if (cancelled) return;
        clearPersistedSession();
        setUser(null);
      });
    return () => { cancelled = true; };
  }, []);

  const value = useMemo<AuthState>(
    () => ({
      user,
      login: (u) => {
        persistUser(u);
        setUser(u);
      },
      updateSession: (u) => {
        const merged = { ...(user ?? {}), ...u, token: u.token ?? user?.token ?? '' } as LoginResponse;
        persistUser(merged);
        setUser(merged);
      },
      logout: () => {
        api.logout().catch(() => {});
        clearPersistedSession();
        setUser(null);
      },
    }),
    [user],
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth outside provider');
  return ctx;
}
