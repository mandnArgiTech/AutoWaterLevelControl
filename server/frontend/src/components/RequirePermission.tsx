import { Navigate } from 'react-router-dom';
import { useAuth } from '../auth/AuthContext';
import type { PermissionKey } from '../api/types';

export function hasPermission(permissions: string[] | undefined, key: PermissionKey | PermissionKey[]): boolean {
  if (!permissions) return false;
  const keys = Array.isArray(key) ? key : [key];
  return keys.some((k) => permissions.includes(k));
}

export default function RequirePermission({
  permission,
  children,
}: {
  permission: PermissionKey | PermissionKey[];
  children: React.ReactNode;
}) {
  const { user } = useAuth();
  if (!user) return <Navigate to="/login" replace />;
  if (!hasPermission(user.permissions, permission)) {
    return (
      <div className="page">
        <div className="card-panel">
          <h1>Access denied</h1>
          <p>You do not have permission to view this page.</p>
        </div>
      </div>
    );
  }
  return <>{children}</>;
}
