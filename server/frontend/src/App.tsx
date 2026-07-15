import { lazy, Suspense, type ReactNode } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { useAuth } from './auth/AuthContext';
import RequirePermission from './components/RequirePermission';
import LoginPage from './pages/LoginPage';
import ChangePasswordPage from './pages/ChangePasswordPage';
import DashboardPage from './pages/DashboardPage';
import Layout from './components/Layout';

const ReportsPage = lazy(() => import('./pages/ReportsPage'));
const UsersPage = lazy(() => import('./pages/UsersPage'));
const AdminVendorsPage = lazy(() => import('./pages/AdminVendorsPage'));
const RolesPermissionsPage = lazy(() => import('./pages/admin/RolesPermissionsPage'));
const AdminUsersPage = lazy(() => import('./pages/admin/AdminUsersPage'));
const AdminMqttPage = lazy(() => import('./pages/admin/AdminMqttPage'));
const AdminDatabasePage = lazy(() => import('./pages/admin/AdminDatabasePage'));
const AdminSqlWorkspacePage = lazy(() => import('./pages/admin/AdminSqlWorkspacePage'));
const AdminAuditPage = lazy(() => import('./pages/admin/AdminAuditPage'));

function PrivateRoute({ children }: { children: ReactNode }) {
  const { user } = useAuth();
  if (!user) return <Navigate to="/login" replace />;
  if (user.mustChangePassword) return <Navigate to="/change-password" replace />;
  return <>{children}</>;
}

function PageFallback() {
  return <div className="loading-panel">Loading…</div>;
}

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<LoginPage />} />
      <Route path="/change-password" element={<ChangePasswordPage />} />
      <Route
        path="/*"
        element={
          <PrivateRoute>
            <Layout>
              <Suspense fallback={<PageFallback />}>
                <Routes>
                  <Route path="/" element={<DashboardPage />} />
                  <Route path="/reports" element={<ReportsPage />} />
                  <Route path="/users" element={<UsersPage />} />
                  <Route path="/admin/vendors" element={
                    <RequirePermission permission="VENDOR_MANAGE"><AdminVendorsPage /></RequirePermission>
                  } />
                  <Route path="/admin/roles" element={
                    <RequirePermission permission="ROLE_MANAGE"><RolesPermissionsPage /></RequirePermission>
                  } />
                  <Route path="/admin/users" element={
                    <RequirePermission permission="USER_MANAGE"><AdminUsersPage /></RequirePermission>
                  } />
                  <Route path="/admin/mqtt" element={
                    <RequirePermission permission={['PLATFORM_MQTT_READ', 'PLATFORM_MQTT_WRITE']}><AdminMqttPage /></RequirePermission>
                  } />
                  <Route path="/admin/database" element={
                    <RequirePermission permission={['PLATFORM_DB_READ', 'PLATFORM_DB_BACKUP']}><AdminDatabasePage /></RequirePermission>
                  } />
                  <Route path="/admin/sql" element={
                    <RequirePermission permission="PLATFORM_SQL_READ"><AdminSqlWorkspacePage /></RequirePermission>
                  } />
                  <Route path="/admin/audit" element={
                    <RequirePermission permission="AUDIT_READ"><AdminAuditPage /></RequirePermission>
                  } />
                </Routes>
              </Suspense>
            </Layout>
          </PrivateRoute>
        }
      />
    </Routes>
  );
}
