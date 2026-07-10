import { Navigate, Route, Routes } from 'react-router-dom';
import { useAuth } from './auth/AuthContext';
import RequirePermission from './components/RequirePermission';
import LoginPage from './pages/LoginPage';
import ChangePasswordPage from './pages/ChangePasswordPage';
import DashboardPage from './pages/DashboardPage';
import ReportsPage from './pages/ReportsPage';
import UsersPage from './pages/UsersPage';
import AdminVendorsPage from './pages/AdminVendorsPage';
import RolesPermissionsPage from './pages/admin/RolesPermissionsPage';
import AdminUsersPage from './pages/admin/AdminUsersPage';
import AdminMqttPage from './pages/admin/AdminMqttPage';
import AdminDatabasePage from './pages/admin/AdminDatabasePage';
import AdminSqlWorkspacePage from './pages/admin/AdminSqlWorkspacePage';
import AdminAuditPage from './pages/admin/AdminAuditPage';
import Layout from './components/Layout';

function PrivateRoute({ children }: { children: React.ReactNode }) {
  const { user } = useAuth();
  if (!user) return <Navigate to="/login" replace />;
  if (user.mustChangePassword) return <Navigate to="/change-password" replace />;
  return <>{children}</>;
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
            </Layout>
          </PrivateRoute>
        }
      />
    </Routes>
  );
}
