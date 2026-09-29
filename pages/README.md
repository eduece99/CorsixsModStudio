# Corsix's Mod Studio website

The Vite + React + HeroUI website lives entirely in this directory. Run `npm ci` and `npm run dev` here to develop locally, or `npm run build` to produce `dist/`. The site is designed for the project URL `https://jbelford.github.io/CorsixsModStudio/`; update `base` in `vite.config.ts` if the repository name or Pages URL changes.

The only deployment file outside `pages/` is `.github/workflows/pages.yml`, because GitHub Actions requires workflows in `.github/workflows/`. To publish, select **GitHub Actions** under **Settings → Pages → Build and deployment → Source**. Push to `master` to build and deploy, or run the workflow manually.

Product descriptions and installation instructions follow the [project README](../README.md). The site is dark-only and does not use screenshots or simulated application UI.
