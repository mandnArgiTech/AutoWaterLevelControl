import { createContext, useContext, useMemo, useState, type ReactNode } from 'react';
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
    localStorage.setItem('flm_token', u.token);
  }
  localStorage.setItem('flm_user', JSON.stringify(u));
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<LoginResponse | null>(() => {
    const raw = localStorage.getItem('flm_user');
    return raw ? JSON.parse(raw) : null;
  });

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
        localStorage.removeItem('flm_token');
        localStorage.removeItem('flm_user');
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
