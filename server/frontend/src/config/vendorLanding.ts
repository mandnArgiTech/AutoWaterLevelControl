/** Landing copy keyed by vendorType (FLM-F.8) — one app, config map. */
export type VendorLandingProfile = {
  eyebrow: string;
  title: string;
  lead: string;
  tankLabel: string;
  avgLabel: string;
};

const PROFILES: Record<string, VendorLandingProfile> = {
  HOUSEHOLD: {
    eyebrow: 'Home monitoring',
    title: 'House Water Dashboard',
    lead: 'Live levels for your home tanks and sump',
    tankLabel: 'Total tanks',
    avgLabel: 'Average fill',
  },
  APARTMENT: {
    eyebrow: 'Building monitoring',
    title: 'Building Water Dashboard',
    lead: 'Shared overhead and sump levels across the building',
    tankLabel: 'Monitored tanks',
    avgLabel: 'Average fill',
  },
  WATER_UTILITY: {
    eyebrow: 'Network operations',
    title: 'Utility Water Network',
    lead: 'District tanks and distribution points at a glance',
    tankLabel: 'Network nodes',
    avgLabel: 'Average fill',
  },
  IRRIGATION: {
    eyebrow: 'Field monitoring',
    title: 'Irrigation Field Dashboard',
    lead: 'Reservoir and canal levels for your fields',
    tankLabel: 'Reservoirs',
    avgLabel: 'Average level',
  },
  OTHER: {
    eyebrow: 'Live monitoring',
    title: 'Water Level Dashboard',
    lead: 'Real-time levels from your connected devices',
    tankLabel: 'Total devices',
    avgLabel: 'Average fill',
  },
};

export function landingProfile(vendorType?: string | null): VendorLandingProfile {
  if (vendorType && PROFILES[vendorType]) return PROFILES[vendorType];
  return PROFILES.OTHER;
}
