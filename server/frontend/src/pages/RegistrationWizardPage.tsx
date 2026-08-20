import { useMemo, useState } from 'react';
import { useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from '../api/client';
import { useAuth } from '../auth/AuthContext';
import { landingProfile } from '../config/vendorLanding';

type Step = 0 | 1 | 2 | 3 | 4;

const STEPS = ['Site', 'Assets', 'Flows', 'Device', 'Bind'] as const;

const HOME_TEMPLATE = {
  assets: [
    { key: 'municipal', name: 'Municipal supply', kind: 'SOURCE', subtype: 'MUNICIPAL' },
    { key: 'sump', name: 'Sump', kind: 'TANK', subtype: 'SUMP', capacityL: 2000 },
    { key: 'pump', name: 'Transfer pump', kind: 'PUMP', subtype: 'RELAY' },
    { key: 'overhead', name: 'Overhead tank', kind: 'TANK', subtype: 'OVERHEAD', capacityL: 1000 },
  ],
  flows: [
    { from: 'municipal', to: 'sump', kind: 'FLOW' },
    { from: 'sump', to: 'overhead', via: 'pump', kind: 'FLOW' },
  ],
};

export default function RegistrationWizardPage() {
  const { user } = useAuth();
  const qc = useQueryClient();
  const profile = landingProfile(user?.vendorType);
  const [step, setStep] = useState<Step>(0);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const [siteName, setSiteName] = useState('Home');
  const [siteId, setSiteId] = useState('');
  const [assetMap, setAssetMap] = useState<Record<string, string>>({});
  const [deviceId, setDeviceId] = useState('');
  const [deviceTag, setDeviceTag] = useState('');
  const [displayName, setDisplayName] = useState('');
  const [modelKey, setModelKey] = useState('sensor');
  const [bindAssetKey, setBindAssetKey] = useState('overhead');
  const [capabilityKey, setCapabilityKey] = useState('measure.water_level');
  const [doneMsg, setDoneMsg] = useState<string | null>(null);

  const sites = useQuery({ queryKey: ['sites'], queryFn: api.sites });
  const pending = useQuery({ queryKey: ['pending'], queryFn: api.pendingDevices });
  const models = useQuery({ queryKey: ['device-models'], queryFn: api.deviceModels });
  const caps = useQuery({ queryKey: ['capabilities'], queryFn: api.capabilities });

  const vendorId = user?.vendorId ?? '';

  const canAdvance = useMemo(() => {
    if (step === 0) return !!siteId || siteName.trim().length > 0;
    if (step === 1 || step === 2) return !!siteId;
    if (step === 3) return !!deviceId || (!!deviceTag && !!vendorId);
    if (step === 4) return !!deviceId && !!assetMap[bindAssetKey];
    return false;
  }, [step, siteId, siteName, deviceId, deviceTag, vendorId, assetMap, bindAssetKey]);

  async function createOrSelectSite() {
    setError(null);
    setBusy(true);
    try {
      if (siteId) {
        setStep(1);
        return;
      }
      if (!vendorId) throw new Error('No vendor on session — login as a vendor user');
      const site = await api.createSite({
        vendorId,
        name: siteName.trim(),
        kind: user?.vendorType === 'IRRIGATION' ? 'FARM' : 'HOME',
      });
      setSiteId(site.id);
      await qc.invalidateQueries({ queryKey: ['sites'] });
      setStep(1);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to create site');
    } finally {
      setBusy(false);
    }
  }

  async function provisionAssets() {
    setError(null);
    setBusy(true);
    try {
      const result = await api.provisionSite(siteId, HOME_TEMPLATE);
      const assets = (result.assets ?? {}) as Record<string, string>;
      setAssetMap(assets);
      setStep(2);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Provision failed');
    } finally {
      setBusy(false);
    }
  }

  async function confirmFlows() {
    setError(null);
    setStep(3);
  }

  async function registerOrPickDevice() {
    setError(null);
    setBusy(true);
    try {
      if (deviceId) {
        setStep(4);
        return;
      }
      if (!vendorId || !deviceTag.trim()) throw new Error('Pick a pending device or enter a tag');
      const d = await api.registerDevice({
        vendorId,
        deviceTag: deviceTag.trim(),
        displayName: displayName.trim() || deviceTag.trim(),
        modelKey,
        siteId,
        commType: 'wifi',
      });
      setDeviceId(d.id);
      await qc.invalidateQueries({ queryKey: ['devices'] });
      await qc.invalidateQueries({ queryKey: ['pending'] });
      setStep(4);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Register failed');
    } finally {
      setBusy(false);
    }
  }

  async function bindChannel() {
    setError(null);
    setBusy(true);
    try {
      const assetId = assetMap[bindAssetKey];
      if (!assetId) throw new Error('Asset missing — re-run Assets step');
      const channel = await api.ensureChannel(deviceId, {
        capabilityKey,
        chanIndex: 0,
        name: capabilityKey,
      });
      const role = capabilityKey.startsWith('control.') ? 'ACTUATES' : 'MEASURES';
      await api.createBinding({
        assetId,
        nodeCapabilityId: channel.id,
        role,
      });
      setDoneMsg(`Bound ${capabilityKey} → ${bindAssetKey} (${role})`);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Bind failed');
    } finally {
      setBusy(false);
    }
  }

  async function next() {
    if (step === 0) return createOrSelectSite();
    if (step === 1) return provisionAssets();
    if (step === 2) return confirmFlows();
    if (step === 3) return registerOrPickDevice();
    if (step === 4) return bindChannel();
  }

  return (
    <div className="page wizard-page">
      <header className="page-header">
        <div>
          <p className="eyebrow">{profile.eyebrow} · setup</p>
          <h1>Registration wizard</h1>
          <p className="page-lead">
            Site → assets → flows → device → bind. Defaults follow your vendor profile.
          </p>
        </div>
      </header>

      <ol className="wizard-steps">
        {STEPS.map((label, i) => (
          <li key={label} className={i === step ? 'active' : i < step ? 'done' : ''}>
            <span>{i + 1}</span>
            {label}
          </li>
        ))}
      </ol>

      {error && <div className="alert alert-error">{error}</div>}
      {doneMsg && <div className="alert alert-ok">{doneMsg}</div>}

      <section className="wizard-panel">
        {step === 0 && (
          <>
            <h2>Choose or create a site</h2>
            <label className="field">
              <span>Existing site</span>
              <select
                value={siteId}
                onChange={(e) => setSiteId(e.target.value)}
              >
                <option value="">— create new —</option>
                {(sites.data ?? []).map((s) => (
                  <option key={s.id} value={s.id}>{s.name} ({s.kind})</option>
                ))}
              </select>
            </label>
            {!siteId && (
              <label className="field">
                <span>New site name</span>
                <input value={siteName} onChange={(e) => setSiteName(e.target.value)} />
              </label>
            )}
          </>
        )}

        {step === 1 && (
          <>
            <h2>Assets (Home template)</h2>
            <p className="page-lead">
              Creates municipal → sump → pump → overhead. You can adjust later via API.
            </p>
            <ul className="wizard-list">
              {HOME_TEMPLATE.assets.map((a) => (
                <li key={a.key}><strong>{a.name}</strong> — {a.kind}/{a.subtype ?? '—'}</li>
              ))}
            </ul>
          </>
        )}

        {step === 2 && (
          <>
            <h2>Flows</h2>
            <p className="page-lead">Edges created with the template:</p>
            <ul className="wizard-list">
              {HOME_TEMPLATE.flows.map((f, i) => (
                <li key={i}>{f.from} → {f.to}{f.via ? ` via ${f.via}` : ''}</li>
              ))}
            </ul>
            {Object.keys(assetMap).length > 0 && (
              <p className="stat-hint">Asset IDs ready ({Object.keys(assetMap).length}).</p>
            )}
          </>
        )}

        {step === 3 && (
          <>
            <h2>Register device</h2>
            <label className="field">
              <span>Pending MQTT device</span>
              <select
                value={deviceTag}
                onChange={(e) => {
                  const tag = e.target.value;
                  setDeviceTag(tag);
                  const p = (pending.data ?? []).find((x) => x.deviceTag === tag);
                  if (p?.modelKey) setModelKey(p.modelKey);
                  setDisplayName(tag);
                  setDeviceId('');
                }}
              >
                <option value="">— enter manually —</option>
                {(pending.data ?? []).map((p) => (
                  <option key={p.deviceTag} value={p.deviceTag}>
                    {p.deviceTag}{p.modelKey ? ` (${p.modelKey})` : ''}
                  </option>
                ))}
              </select>
            </label>
            <label className="field">
              <span>Device tag</span>
              <input value={deviceTag} onChange={(e) => setDeviceTag(e.target.value)} />
            </label>
            <label className="field">
              <span>Display name</span>
              <input value={displayName} onChange={(e) => setDisplayName(e.target.value)} />
            </label>
            <label className="field">
              <span>Model</span>
              <select value={modelKey} onChange={(e) => setModelKey(e.target.value)}>
                {(models.data ?? [{ modelKey: 'sensor', name: 'sensor' }]).map((m) => (
                  <option key={m.modelKey} value={m.modelKey}>{m.name} ({m.modelKey})</option>
                ))}
              </select>
            </label>
          </>
        )}

        {step === 4 && (
          <>
            <h2>Bind channel to asset</h2>
            <label className="field">
              <span>Asset</span>
              <select value={bindAssetKey} onChange={(e) => setBindAssetKey(e.target.value)}>
                {Object.keys(assetMap).map((k) => (
                  <option key={k} value={k}>{k}</option>
                ))}
              </select>
            </label>
            <label className="field">
              <span>Capability</span>
              <select value={capabilityKey} onChange={(e) => setCapabilityKey(e.target.value)}>
                {(caps.data ?? []).map((c) => (
                  <option key={c.key} value={c.key}>{c.name} ({c.key})</option>
                ))}
              </select>
            </label>
          </>
        )}
      </section>

      <div className="wizard-actions">
        <button
          type="button"
          className="btn btn-soft"
          disabled={busy || step === 0}
          onClick={() => setStep((s) => (s > 0 ? ((s - 1) as Step) : s))}
        >
          Back
        </button>
        <button
          type="button"
          className="btn btn-primary"
          disabled={busy || !canAdvance}
          onClick={() => void next()}
        >
          {busy ? 'Working…' : step === 4 ? 'Bind' : 'Continue'}
        </button>
      </div>
    </div>
  );
}
