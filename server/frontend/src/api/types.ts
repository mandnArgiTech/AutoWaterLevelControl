export type UserRole = 'SUPER_ADMIN' | 'VENDOR_ADMIN' | 'OPERATOR' | 'VIEWER';

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
}

export interface VendorSummary {
  id: string;
  code: string;
  name: string;
  active: boolean;
  createdAt: string;
}
