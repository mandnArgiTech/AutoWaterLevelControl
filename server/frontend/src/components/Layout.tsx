import { useEffect, useState } from 'react';
import { Link, useLocation } from 'react-router-dom';
import { useAuth } from '../auth/AuthContext';
import { hasPermission } from './RequirePermission';
import { useIsMobile } from '../hooks/useMediaQuery';
import type { PermissionKey } from '../api/types';

const NAV_ICONS: Record<string, string> = {
  '/': '📊',
  '/reports': '📈',
  '/users': '👥',
  '/admin/vendors': '🏢',
  '/admin/roles': '🔐',
  '/admin/users': '👤',
  '/admin/mqtt': '📡',
  '/admin/database': '🗄️',
  '/admin/sql': '⌨️',
  '/admin/audit': '📋',
};

type NavItem = { to: string; label: string; permission?: PermissionKey | PermissionKey[] };

export default function Layout({ children }: { children: React.ReactNode }) {
  const { user, logout } = useAuth();
  const loc = useLocation();
  const isMobile = useIsMobile();
  const [menuOpen, setMenuOpen] = useState(false);

  const mainNav: NavItem[] = [
    { to: '/', label: 'Dashboard' },
    { to: '/reports', label: 'Reports' },
    ...(user?.role === 'VENDOR_ADMIN' || user?.role === 'SUPER_ADMIN'
      ? [{ to: '/users', label: 'Users' }]
      : []),
  ];

  const adminNav: NavItem[] = [
    { to: '/admin/vendors', label: 'Vendors', permission: 'VENDOR_MANAGE' as PermissionKey },
    { to: '/admin/roles', label: 'Roles', permission: 'ROLE_MANAGE' as PermissionKey },
    { to: '/admin/users', label: 'Platform Users', permission: 'USER_MANAGE' as PermissionKey },
    { to: '/admin/mqtt', label: 'MQTT', permission: ['PLATFORM_MQTT_READ', 'PLATFORM_MQTT_WRITE'] as PermissionKey[] },
    { to: '/admin/database', label: 'Database', permission: ['PLATFORM_DB_READ', 'PLATFORM_DB_BACKUP'] as PermissionKey[] },
    { to: '/admin/sql', label: 'SQL', permission: 'PLATFORM_SQL_READ' as PermissionKey },
    { to: '/admin/audit', label: 'Audit', permission: 'AUDIT_READ' as PermissionKey },
  ].filter((n) => n.permission && hasPermission(user?.permissions, n.permission));

  const nav = [...mainNav, ...adminNav];
  const currentPage = nav.find((n) => n.to === loc.pathname)?.label ?? 'FLM';

  useEffect(() => {
    setMenuOpen(false);
  }, [loc.pathname]);

  useEffect(() => {
    document.body.classList.toggle('nav-open', menuOpen);
    return () => document.body.classList.remove('nav-open');
  }, [menuOpen]);

  function closeMenu() {
    setMenuOpen(false);
  }

  return (
    <div className={`app-shell ${menuOpen ? 'menu-open' : ''}`}>
      {isMobile && (
        <header className="mobile-topbar">
          <button
            type="button"
            className="icon-btn"
            aria-label={menuOpen ? 'Close menu' : 'Open menu'}
            aria-expanded={menuOpen}
            onClick={() => setMenuOpen((o) => !o)}
          >
            <span className={`hamburger ${menuOpen ? 'open' : ''}`} />
          </button>
          <div className="mobile-topbar-title">
            <span className="mobile-topbar-eyebrow">Fluid Level Monitor</span>
            <strong>{currentPage}</strong>
          </div>
          <span className="mobile-topbar-avatar" aria-hidden>
            {user?.displayName?.charAt(0) ?? '?'}
          </span>
        </header>
      )}

      {isMobile && menuOpen && (
        <button
          type="button"
          className="sidebar-backdrop"
          aria-label="Close menu"
          onClick={closeMenu}
        />
      )}

      <aside className="sidebar" aria-label="Main navigation">
        <div className="brand-block">
          <span className="brand-icon">💧</span>
          <div>
            <div className="brand">Fluid Level Monitor</div>
            <div className="brand-sub">Monitoring Platform</div>
          </div>
          {isMobile && (
            <button type="button" className="icon-btn sidebar-close" aria-label="Close menu" onClick={closeMenu}>
              ✕
            </button>
          )}
        </div>
        {user?.vendorName && <div className="vendor-badge">{user.vendorName}</div>}
        <nav>
          {mainNav.map((n) => (
            <Link
              key={n.to}
              to={n.to}
              className={loc.pathname === n.to ? 'active' : ''}
              onClick={closeMenu}
            >
              <span className="nav-icon">{NAV_ICONS[n.to]}</span>
              {n.label}
            </Link>
          ))}
          {adminNav.length > 0 && <div className="nav-section-label">Platform</div>}
          {adminNav.map((n) => (
            <Link
              key={n.to}
              to={n.to}
              className={loc.pathname === n.to ? 'active' : ''}
              onClick={closeMenu}
            >
              <span className="nav-icon">{NAV_ICONS[n.to]}</span>
              {n.label}
            </Link>
          ))}
        </nav>
        <div className="sidebar-footer">
          <div className="user-chip">
            <span className="user-avatar">{user?.displayName?.charAt(0) ?? '?'}</span>
            <div className="user-chip-text">
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

      {isMobile && (
        <nav className="mobile-bottom-nav" aria-label="Quick navigation">
          {mainNav.slice(0, 2).map((n) => (
            <Link
              key={n.to}
              to={n.to}
              className={loc.pathname === n.to ? 'active' : ''}
            >
              <span className="bottom-nav-icon">{NAV_ICONS[n.to]}</span>
              <span>{n.label}</span>
            </Link>
          ))}
          <button type="button" className={menuOpen ? 'active' : ''} onClick={() => setMenuOpen(true)}>
            <span className="bottom-nav-icon">☰</span>
            <span>Menu</span>
          </button>
        </nav>
      )}
    </div>
  );
}
