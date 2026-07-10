import type {
  AuditLogEntry,
  PermissionDto,
  PlatformUserSummary,
  RoleDto,
  UserRole,
} from './types';
import { api } from './client';

const ADMIN = '/admin/platform';

async function adminRequest<T>(
  path: string,
  init?: RequestInit,
  stepUpToken?: string,
): Promise<T> {
  const headers: Record<string, string> = {};
  if (stepUpToken) headers['X-Step-Up-Token'] = stepUpToken;
  const token = localStorage.getItem('flm_token');
  const res = await fetch(`${import.meta.env.VITE_API_BASE ?? '/api'}${path}`, {
    ...init,
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...headers,
      ...(init?.headers as Record<string, string>),
    },
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ message: res.statusText }));
    throw new Error(err.message ?? 'Request failed');
  }
  if (res.status === 204) return undefined as T;
  return res.json();
}

export const adminApi = {
  roles: () => adminRequest<RoleDto[]>(`${ADMIN}/roles`),
  permissions: () => adminRequest<PermissionDto[]>(`${ADMIN}/roles/permissions`),
  updateRolePermissions: (roleId: string, permissionIds: string[], stepUpToken: string) =>
    adminRequest<RoleDto>(`${ADMIN}/roles/${roleId}/permissions`, {
      method: 'PUT',
      body: JSON.stringify(permissionIds),
      headers: { 'X-Step-Up-Token': stepUpToken },
    }),

  platformUsers: () => adminRequest<PlatformUserSummary[]>(`${ADMIN}/users`),
  createPlatformUser: (body: {
    email: string;
    displayName: string;
    password: string;
    role: UserRole;
    vendorId?: string;
  }) =>
    adminRequest<PlatformUserSummary>(`${ADMIN}/users`, {
      method: 'POST',
      body: JSON.stringify(body),
    }),

  mqttStatus: () => adminRequest<Record<string, unknown>>(`${ADMIN}/mqtt/status`),
  mqttClients: () => adminRequest<unknown>(`${ADMIN}/mqtt/clients`),
  createMqttClient: (username: string, password: string) =>
    adminRequest<void>(`${ADMIN}/mqtt/clients`, {
      method: 'POST',
      body: JSON.stringify({ username, password }),
    }),
  deleteMqttClient: (username: string, stepUpToken: string) =>
    adminRequest<void>(`${ADMIN}/mqtt/clients/${encodeURIComponent(username)}`, {
      method: 'DELETE',
      headers: { 'X-Step-Up-Token': stepUpToken },
    }),
  provisionDevice: (username: string, password: string, deviceTag: string) =>
    adminRequest<Record<string, string>>(`${ADMIN}/mqtt/provision-device`, {
      method: 'POST',
      body: JSON.stringify({ username, password, deviceTag }),
    }),

  dbStatus: () => adminRequest<Record<string, unknown>>(`${ADMIN}/database/status`),
  dbBackups: () => adminRequest<Record<string, unknown>[]>(`${ADMIN}/database/backups`),
  createBackup: () => adminRequest<{ filename: string }>(`${ADMIN}/database/backup`, { method: 'POST' }),
  restoreBackup: (filename: string, stepUpToken: string) =>
    adminRequest<void>(`${ADMIN}/database/restore`, {
      method: 'POST',
      body: JSON.stringify({ filename, confirm: true }),
      headers: { 'X-Step-Up-Token': stepUpToken },
    }),

  sqlSchema: () => adminRequest<Record<string, string>[]>(`${ADMIN}/sql/schema`),
  sqlExecute: (sql: string) =>
    adminRequest<Record<string, unknown>>(`${ADMIN}/sql/execute`, {
      method: 'POST',
      body: JSON.stringify({ sql }),
    }),
  sqlExecuteWrite: (sql: string, confirmDestructive: boolean, confirmationPhrase: string, stepUpToken: string) =>
    adminRequest<Record<string, unknown>>(`${ADMIN}/sql/execute-write`, {
      method: 'POST',
      body: JSON.stringify({ sql, confirmDestructive, confirmationPhrase }),
      headers: { 'X-Step-Up-Token': stepUpToken },
    }),

  audit: (page = 0, size = 50) =>
    adminRequest<{ content: AuditLogEntry[]; totalElements: number }>(
      `${ADMIN}/audit?page=${page}&size=${size}`,
    ),

  stepUp: api.stepUp,
};
