import { Card } from "@heroui/react";
import { buttonVariants } from "@heroui/styles";

const repository = "https://github.com/jbelford/CorsixsModStudio";
const releases = `${repository}/releases`;
const issues = `${repository}/issues`;
const originalSite = "https://modstudio.corsix.org/";

const features = [
  ["Modern Windows build", "The original codebase has been updated to C++20 and 64-bit Windows."],
  ["SCAR editing", "LuaLS adds diagnostics, autocomplete, hover information, and go-to-definition, with SCAR API definitions for Dawn of War and Company of Heroes."],
  ["Faster navigation", "Single-click file previews, find-in-file, and familiar shortcuts such as Ctrl+S, Ctrl+Tab, and Ctrl+W."],
  ["Responsiveness and display", "Background loading keeps the UI responsive, and DPI-aware scaling works better on modern displays."],
  ["Dark mode", "A configurable dark theme has been added to the desktop application."],
  ["Dawn of War: Definitive Edition", "Browse and create DE mods, resolve game-install dependencies, and edit pipeline, burn, and archive configuration files."],
];

function App() {
  return (
    <div className="site">
      <header className="site-header">
        <div className="container header-inner">
          <a href="#top" className="site-name">Corsix's Mod Studio <span>DE</span></a>
          <nav aria-label="Main navigation">
            <a href="#about">About</a>
            <a href="#features">What's new</a>
            <a href="#install">Install</a>
            <a href={repository} target="_blank" rel="noopener noreferrer">GitHub ↗</a>
            <a href={issues} target="_blank" rel="noopener noreferrer">Issues ↗</a>
          </nav>
        </div>
      </header>

      <main id="top" className="container">
        <section id="about" className="intro" aria-labelledby="title">
          <h1 id="title">Corsix's Mod Studio:<br />Definitive Edition</h1>
          <p className="lead">An update of Corsix's original Dawn of War and Company of Heroes mod editor for modern Windows.</p>
          <p>The original editor was written by Corsix in the mid-2000s. This project keeps the existing modding workflows and adds the changes listed below. You can find the original project at <a href={originalSite} target="_blank" rel="noopener noreferrer">modstudio.corsix.org ↗</a>.</p>
          <div className="intro-links">
            <a className={`${buttonVariants({ variant: "primary", size: "md" })} download-link`} href={releases} target="_blank" rel="noopener noreferrer">Download latest release</a>
            <a href={repository} target="_blank" rel="noopener noreferrer">View source on GitHub ↗</a>
          </div>
          <p className="small">Windows 10/11 (x64) · ZIP download · Free and open source</p>
        </section>

        <section id="features" className="section" aria-labelledby="features-title">
          <h2 id="features-title">What's new in Definitive Edition</h2>
          <div className="feature-list">
            {features.map(([name, description]) => (
              <div className="feature-row" key={name}>
                <h3>{name}</h3>
                <p>{description}</p>
              </div>
            ))}
          </div>
          <p className="small">The existing RGD, UCS, SGA, and other tools remain available. LuaLS can be turned off from the View menu.</p>
        </section>

        <section id="install" className="section" aria-labelledby="install-title">
          <h2 id="install-title">Getting started</h2>
          <ol className="steps">
            <li>Download the latest <code>win-x64.zip</code> from <a href={releases} target="_blank" rel="noopener noreferrer">GitHub Releases</a>.</li>
            <li>Extract the whole ZIP to a writable folder. Keep <code>Mod_Studio_Files</code> next to <code>ModStudioDE.exe</code>.</li>
            <li>Run <code>ModStudioDE.exe</code>. Use <code>File → Open Mod</code> to open an existing mod, or <code>File → New Mod</code> to create one.</li>
          </ol>
        </section>

        <section className="section" aria-labelledby="de-title">
          <h2 id="de-title">Dawn of War: Definitive Edition</h2>
          <p>First, choose <code>File → Set DoW:DE Game Folder</code> and select your game installation. Then use <code>File → Browse DE Mods</code>, <code>File → Open DoW:DE Mod</code>, or <code>File → New Mod → Dawn of War: Definitive Edition</code>.</p>
          <p>For archives, the DE workflow can run the game's installed <code>Archive.exe</code> with a <code>.sgaconfig</code> file. The original SGA packer remains available for older mods.</p>
          <a href={`${repository}#definitive-edition-mod-workflow`} target="_blank" rel="noopener noreferrer">Read the full DE workflow ↗</a>
        </section>

        <Card className="release-card">
          <Card.Header>
            <Card.Title>Download and documentation</Card.Title>
            <Card.Description>The Windows x64 ZIP and its SHA-256 checksum are published on GitHub Releases. The repository README has build instructions and more detail on the project. Report problems on GitHub Issues.</Card.Description>
          </Card.Header>
          <Card.Content>
            <a href={releases} target="_blank" rel="noopener noreferrer">Releases ↗</a>
            <a href={issues} target="_blank" rel="noopener noreferrer">Report an issue ↗</a>
            <a href={`${repository}/blob/master/README.md`} target="_blank" rel="noopener noreferrer">Project README ↗</a>
            <a href={`${repository}/tree/master/docs`} target="_blank" rel="noopener noreferrer">Format documentation ↗</a>
          </Card.Content>
        </Card>
      </main>

      <footer className="site-footer"><div className="container"><span>Corsix's Mod Studio: Definitive Edition</span><span>Application: GPL v2 · Rainman library: LGPL v2.1</span></div></footer>
    </div>
  );
}

export default App;
