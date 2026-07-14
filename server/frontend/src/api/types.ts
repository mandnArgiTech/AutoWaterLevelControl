export type UserRole = 'SUPER_ADMIN' | 'VENDOR_ADMIN' | 'OPERATOR' | 'VIEWER';

export type PermissionKey =
  | 'PLATFORM_MQTT_READ'
  | 'PLATFORM_MQTT_WRITE'
  | 'PLATFORM_DB_READ'
  | 'PLATFORM_DB_BACKUP'
  | 'PLATFORM_DB_RESTORE'
  | 'PLATFORM_SQL_READ'
  | 'PLATFORM_SQL_WRITE'
  | 'USER_MANAGE'
  | 'ROLE_MANAGE'
  | 'VENDOR_MANAGE'
  | 'AUDIT_READ'
  | 'DEVICE_MANAGE';

export interface LoginResponse {
  token: string;
  userId: string;
  email: string;
  displayName: string;
  role: UserRole;
  vendorId?: string;
  vendorCode?: string;
  vendorName?: string;
  mustChangePassword: boolean;
  permissions?: PermissionKey[];
  mfaRequired?: boolean;
  mfaEnrolled?: boolean;
  mfaProvisioningUri?: string | null;
}

export interface DeviceSummary {
  id: string;
  deviceTag: string;
  displayName: string;
  online: boolean;
  lastSeenAt?: string;
  latestPercentFilled?: number;
  latestVolumeLiters?: number;
  latestReadingAt?: string;
  /** Device system uptime, e.g. "2d 5h 30m 15s" */
  uptime?: string;
  uptimeMs?: number;
}

export interface ReadingPoint {
  id: string;
  receivedAt: string;
  percentFilled?: number;
  volumeLiters?: number;
  temperatureC?: number;
  payloadJson: string;
}

export interface UserSummary {
  id: string;
  email: string;
  displayName: string;
  role: UserRole;
  active: boolean;
  mustChangePassword?: boolean;
}

export interface VendorSummary {
  id: string;
  code: string;
  name: string;
  active: boolean;
  createdAt: string;
}

export interface RoleDto {
  id: string;
  key: string;
  name: string;
  systemManaged: boolean;
  permissions: string[];
}

export interface PermissionDto {
  id: string;
  key: string;
  description: string;
}

export interface PlatformUserSummary {
  id: string;
  email: string;
  displayName: string;
  role: UserRole;
  active: boolean;
  mustChangePassword: boolean;
}

export interface AuditLogEntry {
  id: string;
  actorUserId?: string;
  action: string;
  resource: string;
  detail?: Record<string, unknown>;
  ipAddress?: string;
  createdAt: string;
}
