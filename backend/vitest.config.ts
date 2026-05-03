import { defineConfig } from "vitest/config";

// Vitest config — keep the test surface tight. We only run unit
// tests under test/*.test.ts (pure helpers + crypto). The smoke
// script test/smoke.mjs is run separately via `npm run test:smoke`
// because it hits a deployed Worker and isn't deterministic.
export default defineConfig({
  test: {
    include: ["test/**/*.test.ts"],
    exclude: ["node_modules/**", ".pio/**", "dist/**"],
    environment: "node",
  },
});
