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
    refreshPromise = fetchWithTimeout(`${API_BASE}/auth/refresh`, {
      method: 'POST',
      credentials: 'include',
    }, 8000).then(async (res) => {
      if (!res.ok) throw new Error('Session expired');
      const data = (await res.json()) as LoginResponse;
      setAccessToken(data.token);
      return data;
    }).finally(() => { refreshPromise = null; });
  }
  return refreshPromise;
}

async function fetchWithTimeout(input: string, init: RequestInit, timeoutMs: number): Promise<Response> {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(input, { ...init, signal: controller.signal });
  } finally {
    window.clearTimeout(timer);
  }
}

async function request<T>(path: string, init?: RequestInit, retry = true): Promise<T> {
  const res = await fetchWithTimeout(`${API_BASE}${path}`, {
    ...init,
    cache: 'no-store',
    credentials: 'include',
    headers: authHeaders(init?.headers as HeadersInit),
  }, 15000);
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
    request<{ status: string; command?: string; topic?: string; deviceTag?: string }>(
      `/devices/${deviceId}/command`,
      {
        method: 'POST',
        body: JSON.stringify({ command }),
      },
    ),

  sites: () => request<import('./types').SiteSummary[]>('/sites'),
  createSite: (body: object) =>
    request<import('./types').SiteSummary>('/sites', { method: 'POST', body: JSON.stringify(body) }),

  deviceModels: () => request<import('./types').DeviceModelSummary[]>('/device-models'),
  capabilities: () => request<import('./types').CapabilitySummary[]>('/capabilities'),
  pendingDevices: () => request<Array<{
    deviceTag: string;
    chipId?: string;
    modelKey?: string;
    topicPrefix?: string;
    sampleCount: number;
    lastSeenAt: string;
  }>>('/devices/pending'),

  registerDevice: (body: object) =>
    request<DeviceSummary>('/devices', { method: 'POST', body: JSON.stringify(body) }),

  siteAssets: (siteId: string) =>
    request<import('./types').AssetSummary[]>(`/sites/${siteId}/assets`),
  createAsset: (siteId: string, body: object) =>
    request<import('./types').AssetSummary>(`/sites/${siteId}/assets`, {
      method: 'POST', body: JSON.stringify(body),
    }),
  siteFlows: (siteId: string) =>
    request<import('./types').FlowEdgeSummary[]>(`/sites/${siteId}/flows`),
  createFlow: (siteId: string, body: object) =>
    request<import('./types').FlowEdgeSummary>(`/sites/${siteId}/flows`, {
      method: 'POST', body: JSON.stringify(body),
    }),
  provisionSite: (siteId: string, body: object) =>
    request<Record<string, unknown>>(`/sites/${siteId}/provision`, {
      method: 'POST', body: JSON.stringify(body),
    }),

  ensureChannel: (deviceId: string, body: object) =>
    request<{ id: string; capabilityKey: string }>(`/devices/${deviceId}/channels`, {
      method: 'POST', body: JSON.stringify(body),
    }),
  deviceChannels: (deviceId: string) =>
    request<Array<{ id: string; capabilityKey: string; chanIndex: number; name?: string }>>(
      `/devices/${deviceId}/channels`,
    ),
  createBinding: (body: object) =>
    request<import('./types').BindingSummary>('/bindings', {
      method: 'POST', body: JSON.stringify(body),
    }),
};
