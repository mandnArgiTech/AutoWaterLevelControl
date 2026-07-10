import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { useState } from 'react';
import { adminApi } from '../../api/admin';
import StepUpModal from '../../components/StepUpModal';

export default function RolesPermissionsPage() {
  const qc = useQueryClient();
  const roles = useQuery({ queryKey: ['admin-roles'], queryFn: adminApi.roles });
  const perms = useQuery({ queryKey: ['admin-perms'], queryFn: adminApi.permissions });
  const [selectedRole, setSelectedRole] = useState<string | null>(null);
  const [selectedPerms, setSelectedPerms] = useState<Set<string>>(new Set());
  const [stepUpOpen, setStepUpOpen] = useState(false);

  const role = roles.data?.find((r) => r.id === selectedRole);

  function selectRole(id: string) {
    setSelectedRole(id);
    const r = roles.data?.find((x) => x.id === id);
    if (r) {
      const permIds = new Set(
        (perms.data ?? []).filter((p) => r.permissions.includes(p.key)).map((p) => p.id),
      );
      setSelectedPerms(permIds);
    }
  }

  const save = useMutation({
    mutationFn: ({ token }: { token: string }) =>
      adminApi.updateRolePermissions(selectedRole!, [...selectedPerms], token),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['admin-roles'] });
    },
  });

  return (
    <div className="page">
      <header className="page-header">
        <div>
          <p className="eyebrow">Platform administration</p>
          <h1>Roles &amp; Permissions</h1>
          <p className="page-lead">Manage who can access MQTT, database, and user administration</p>
        </div>
      </header>

      <div className="admin-grid-2">
        <section className="card-panel">
          <h2 className="panel-title">Roles</h2>
          <ul className="admin-list">
            {(roles.data ?? []).map((r) => (
              <li key={r.id}>
                <button
                  type="button"
                  className={`admin-list-btn ${selectedRole === r.id ? 'active' : ''}`}
                  onClick={() => selectRole(r.id)}
                >
                  <strong>{r.name}</strong>
                  <span className="muted">{r.key}</span>
                </button>
              </li>
            ))}
          </ul>
        </section>

        <section className="card-panel">
          <h2 className="panel-title">Permissions {role ? `— ${role.name}` : ''}</h2>
          {!role ? (
            <p className="muted">Select a role to edit permissions.</p>
          ) : (
            <>
              <div className="perm-grid">
                {(perms.data ?? []).map((p) => (
                  <label key={p.id} className="perm-check">
                    <input
                      type="checkbox"
                      checked={selectedPerms.has(p.id)}
                      disabled={role.systemManaged && role.key === 'SUPER_ADMIN' && p.key === 'ROLE_MANAGE'}
                      onChange={(e) => {
                        const next = new Set(selectedPerms);
                        if (e.target.checked) next.add(p.id);
                        else next.delete(p.id);
                        setSelectedPerms(next);
                      }}
                    />
                    <span>
                      <strong>{p.key}</strong>
                      <small>{p.description}</small>
                    </span>
                  </label>
                ))}
              </div>
              <button
                type="button"
                className="btn-primary"
                disabled={!selectedRole}
                onClick={() => setStepUpOpen(true)}
              >
                Save permissions
              </button>
            </>
          )}
        </section>
      </div>

      <StepUpModal
        open={stepUpOpen}
        onClose={() => setStepUpOpen(false)}
        onSuccess={(token) => save.mutate({ token })}
      />
    </div>
  );
}
