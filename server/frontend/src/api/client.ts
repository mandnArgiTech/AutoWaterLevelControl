import type {
  DeviceSummary,
  LoginResponse,
  ReadingPoint,
  UserSummary,
  VendorSummary,
} from './types';

export type {
  DeviceSummary,
  LoginResponse,
  ReadingPoint,
  UserSummary,
  VendorSummary,
  UserRole,
  PermissionKey,
  RoleDto,
  PermissionDto,
  PlatformUserSummary,
  AuditLogEntry,
} from './types';

const API_BASE = import.meta.env.VITE_API_BASE ?? '/api';

let accessToken: string | null = localStorage.getItem('flm_token');
let refreshPromise: Promise<LoginResponse> | null = null;

export function setAccessToken(token: string | null) {
  accessToken = token;
  if (token) localStorage.setItem('flm_token', token);
  else localStorage.removeItem('flm_token');
}

function authHeaders(extra?: HeadersInit): HeadersInit {
  const h: Record<string, string> = { 'Content-Type': 'application/json' };
  if (accessToken) h.Authorization = `Bearer ${accessToken}`;
  return { ...h, ...extra };
}

async function refreshSession(): Promise<LoginResponse> {
  if (!refreshPromise) {
    refreshPromise = fetch(`${API_BASE}/auth/refresh`, {
      method: 'POST',
      credentials: 'include',
    }).then(async (res) => {
      if (!res.ok) throw new Error('Session expired');
      const data = (await res.json()) as LoginResponse;
      setAccessToken(data.token);
      return data;
    }).finally(() => { refreshPromise = null; });
  }
  return refreshPromise;
}

async function request<T>(path: string, init?: RequestInit, retry = true): Promise<T> {
  const res = await fetch(`${API_BASE}${path}`, {
    ...init,
    cache: 'no-store',
    credentials: 'include',
    headers: authHeaders(init?.headers as HeadersInit),
  });
  if (res.status === 401 && retry && !path.includes('/auth/login')) {
    await refreshSession();
    return request<T>(path, init, false);
  }
  if (!res.ok) {
    const err = await res.json().catch(() => ({ message: res.statusText }));
    throw new Error(err.message ?? 'Request failed');
  }
  if (res.status === 204) return undefined as T;
  return res.json();
}

export const api = {
  login: (username: string, password: string, vendorCode?: string, otpCode?: string) =>
    request<LoginResponse>('/auth/login', {
      method: 'POST',
      body: JSON.stringify({ username, password, vendorCode: vendorCode || null, otpCode: otpCode || null }),
    }, false).then((data) => {
      setAccessToken(data.token);
      return data;
    }),

  logout: () =>
    request<void>('/auth/logout', { method: 'POST' }, false).finally(() => setAccessToken(null)),

  changePassword: (currentPassword: string, newPassword: string) =>
    request<LoginResponse>('/auth/change-password', {
      method: 'POST',
      body: JSON.stringify({ currentPassword, newPassword }),
    }).then((data) => {
      setAccessToken(data.token);
      return data;
    }),

  me: () => request<LoginResponse>('/auth/me'),

  stepUp: (password: string, otpCode?: string) =>
    request<{ stepUpToken: string }>('/auth/step-up', {
      method: 'POST',
      body: JSON.stringify({ password, otpCode }),
    }),

  enrollMfa: () => request<{ provisioningUri: string }>('/auth/mfa/enroll', { method: 'POST' }),

  confirmMfa: (otpCode: string) =>
    request<void>('/auth/mfa/confirm', { method: 'POST', body: JSON.stringify({ otpCode }) }),

  devices: () => request<DeviceSummary[]>('/devices'),

  deviceReadings: (deviceId: string, page = 0, size = 100) =>
    request<{ items: ReadingPoint[]; total: number }>(
      `/devices/${deviceId}/readings?page=${page}&size=${size}`,
    ),

  vendorReport: (vendorId: string, from: string, to: string) =>
    request<ReadingPoint[]>(`/reports/vendor/${vendorId}?from=${from}&to=${to}`),

  vendors: () => request<VendorSummary[]>('/admin/vendors'),

  createVendor: (code: string, name: string) =>
    request<VendorSummary>('/admin/vendors', {
      method: 'POST',
      body: JSON.stringify({ code, name }),
    }),

  vendorUsers: (vendorId: string) => request<UserSummary[]>(`/vendors/${vendorId}/users`),

  createUser: (vendorId: string, body: object) =>
    request<UserSummary>(`/vendors/${vendorId}/users`, {
      method: 'POST',
      body: JSON.stringify(body),
    }),

  deviceCommand: (deviceId: string, command: string) =>
    request<{ status: string }>(`/devices/${deviceId}/command`, {
      method: 'POST',
      body: JSON.stringify({ command }),
    }),
};
