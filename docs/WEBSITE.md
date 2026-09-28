# Website

The static homepage lives in `site/`. It uses the project's existing screenshots
and icon and has no package or build step.

## Preview locally

From the repository root:

```sh
python3 -m http.server 8000 --directory site
```

Open <http://localhost:8000/>.

## Publish with GitHub Pages

In the repository's **Settings → Pages**, choose **GitHub Actions** as the build
and deployment source. The `Publish website` workflow deploys `site/` when it
changes on `main`; it can also be run manually from the Actions tab.

GitHub Pages derives the project URL from the repository name. This repository
currently publishes at `https://chrislauinger77.github.io/QontrolPanel/`.
To publish at `/qontolpanel/`, use a repository with that name for the Pages
site or change the repository name. The homepage itself uses relative asset
paths and works at either address.
