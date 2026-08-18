-- Extended metadata: nullable cover_url only (v1).
PRAGMA foreign_keys = ON;
BEGIN TRANSACTION;
ALTER TABLE games ADD COLUMN cover_url TEXT;
INSERT OR REPLACE INTO merge_policy (field_name, rule_order, rule_key, rule_description, active)
VALUES (
    'cover_url',
    1,
    'higher_priority_source',
    'Prefer cover_url from highest-priority source.',
    1
);
COMMIT;
