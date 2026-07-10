import type { DeviceSummary, LoginResponse, ReadingPoint, UserSummary, VendorSummary } from './types';

export type { DeviceSummary, LoginResponse, ReadingPoint, UserSummary, VendorSummary, UserRole } from './types';

const API_BASE = import.meta.env.VITE_API_BASE ?? '/api';

function authHeaders(): HeadersInit {
  const token = localStorage.getItem('flm_token');
  return token ? { Authorization: `Bearer ${token}` } : {};
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${API_BASE}${path}`, {
    ...init,
    headers: {
      'Content-Type': 'application/json',
      ...authHeaders(),
      ...init?.headers,
    },
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ message: res.statusText }));
    throw new Error(err.message ?? 'Request failed');
  }
  return res.json();
}

export const api = {
  login: (username: string, password: string, vendorCode?: string) =>
    request<LoginResponse>('/auth/login', {
      method: 'POST',
      body: JSON.stringify({ username, password, vendorCode: vendorCode || null }),
    }),

  changePassword: (currentPassword: string, newPassword: string) =>
    request<LoginResponse>('/auth/change-password', {
      method: 'POST',
      body: JSON.stringify({ currentPassword, newPassword }),
    }),

  devices: () => request<DeviceSummary[]>('/devices'),

  deviceReadings: (deviceId: string, page = 0, size = 100) =>
    request<{ items: ReadingPoint[]; total: number }>(
      `/devices/${deviceId}/readings?page=${page}&size=${size}`,
    ),

  vendorReport: (vendorId: string, from: string, to: string) =>
    request<ReadingPoint[]>(`/reports/vendor/${vendorId}?from=${from}&to=${to}`),

  vendors: () => request<VendorSummary[]>('/admin/vendors'),

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
