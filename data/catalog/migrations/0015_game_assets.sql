-- Multi-type artwork URLs (HTTPS only; bytes stay in cache).
-- asset_type: boxart | snap | title | logo | hero | banner
CREATE TABLE IF NOT EXISTS game_assets (
    game_id TEXT NOT NULL,
    asset_type TEXT NOT NULL,
    url TEXT NOT NULL,
    source TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (game_id, asset_type),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_game_assets_type ON game_assets(asset_type);
