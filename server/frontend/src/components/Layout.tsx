import { Link, useLocation } from 'react-router-dom';
import { useAuth } from '../auth/AuthContext';

const NAV_ICONS: Record<string, string> = {
  '/': '📊',
  '/reports': '📈',
  '/users': '👥',
  '/admin/vendors': '🏢',
};

export default function Layout({ children }: { children: React.ReactNode }) {
  const { user, logout } = useAuth();
  const loc = useLocation();

  const nav = [
    { to: '/', label: 'Dashboard' },
    { to: '/reports', label: 'Reports' },
    ...(user?.role === 'VENDOR_ADMIN' || user?.role === 'SUPER_ADMIN'
      ? [{ to: '/users', label: 'Users' }]
      : []),
    ...(user?.role === 'SUPER_ADMIN' ? [{ to: '/admin/vendors', label: 'Vendors' }] : []),
  ];

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand-block">
          <span className="brand-icon">💧</span>
          <div>
            <div className="brand">Fluid Level Monitor</div>
            <div className="brand-sub">Monitoring Platform</div>
          </div>
        </div>
        {user?.vendorName && <div className="vendor-badge">{user.vendorName}</div>}
        <nav>
          {nav.map((n) => (
            <Link key={n.to} to={n.to} className={loc.pathname === n.to ? 'active' : ''}>
              <span className="nav-icon">{NAV_ICONS[n.to]}</span>
              {n.label}
            </Link>
          ))}
        </nav>
        <div className="sidebar-footer">
          <div className="user-chip">
            <span className="user-avatar">{user?.displayName?.charAt(0) ?? '?'}</span>
            <div>
              <div className="user-name">{user?.displayName}</div>
              <div className="user-email">@{user?.email}</div>
            </div>
          </div>
          <button type="button" className="btn btn-ghost btn-block" onClick={logout}>
            Sign out
          </button>
        </div>
      </aside>
      <main className="content">{children}</main>
    </div>
  );
}
