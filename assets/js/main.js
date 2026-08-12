// AsterOS site — shared interactivity. No frameworks, no build step.
(() => {
  "use strict";

  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ---------------- Nav: scroll shadow, mobile toggle, active link ---------------- */
  const nav = document.querySelector(".site-nav");
  const toggle = document.querySelector(".nav-toggle");
  const links = document.querySelector(".nav-links");

  if (nav) {
    const onScroll = () => nav.classList.toggle("scrolled", window.scrollY > 8);
    onScroll();
    window.addEventListener("scroll", onScroll, { passive: true });
  }

  if (toggle && links) {
    toggle.addEventListener("click", () => {
      const open = links.classList.toggle("open");
      toggle.classList.toggle("open", open);
      toggle.setAttribute("aria-expanded", String(open));
    });
    links.querySelectorAll("a").forEach((a) =>
      a.addEventListener("click", () => {
        links.classList.remove("open");
        toggle.classList.remove("open");
      })
    );
  }

  const here = location.pathname.split("/").pop() || "index.html";
  document.querySelectorAll(".nav-links a[href]").forEach((a) => {
    const href = a.getAttribute("href");
    if (href === here || (here === "" && href === "index.html")) a.classList.add("active");
  });

  /* ---------------- Cursor glow ---------------- */
  const glow = document.querySelector(".cursor-glow");
  if (glow && !reduceMotion) {
    let raf = null;
    window.addEventListener(
      "pointermove",
      (e) => {
        glow.classList.add("active");
        if (raf) return;
        raf = requestAnimationFrame(() => {
          glow.style.transform = `translate(${e.clientX}px, ${e.clientY}px)`;
          raf = null;
        });
      },
      { passive: true }
    );
    window.addEventListener("pointerleave", () => glow.classList.remove("active"));
  }

  /* ---------------- Scroll reveal ---------------- */
  const revealEls = document.querySelectorAll(".reveal, .reveal-group > *");
  if (revealEls.length) {
    if (reduceMotion) {
      revealEls.forEach((el) => el.classList.add("in"));
    } else {
      const io = new IntersectionObserver(
        (entries) => {
          entries.forEach((entry) => {
            if (entry.isIntersecting) {
              entry.target.classList.add("in");
              io.unobserve(entry.target);
            }
          });
        },
        { threshold: 0.12, rootMargin: "0px 0px -60px 0px" }
      );
      revealEls.forEach((el, i) => {
        el.style.setProperty("--i", i % 8);
        io.observe(el);
      });
    }
  }

  /* ---------------- Boot terminal typewriter (home hero) ---------------- */
  const term = document.querySelector("[data-terminal]");
  if (term) {
    const lines = Array.from(term.querySelectorAll(".ln"));
    const caretHost = term.querySelector(".caret-host");

    const revealLine = (el) =>
      new Promise((resolve) => {
        el.classList.add("shown");
        if (reduceMotion) return resolve();
        const full = el.dataset.text ?? el.textContent;
        el.innerHTML = "";
        let i = 0;
        const speed = el.dataset.speed ? Number(el.dataset.speed) : 14;
        const tick = () => {
          el.innerHTML = full.slice(0, i) + '<span class="caret"></span>';
          i++;
          if (i <= full.length) {
            setTimeout(tick, speed);
          } else {
            el.innerHTML = full;
            resolve();
          }
        };
        tick();
      });

    const run = async () => {
      for (const line of lines) {
        const pause = line.dataset.pause ? Number(line.dataset.pause) : 90;
        await revealLine(line);
        await new Promise((r) => setTimeout(r, pause));
      }
      if (caretHost) caretHost.innerHTML = '<span class="caret"></span>';
    };

    const io = new IntersectionObserver((entries) => {
      if (entries[0].isIntersecting) {
        run();
        io.disconnect();
      }
    });
    io.observe(term);
  }

  /* ---------------- Architecture stack: click-to-expand layers ---------------- */
  document.querySelectorAll(".layer").forEach((layer) => {
    layer.addEventListener("click", () => {
      const willOpen = !layer.classList.contains("active");
      layer.closest(".stack")?.querySelectorAll(".layer.active").forEach((l) => {
        if (l !== layer) l.classList.remove("active");
      });
      layer.classList.toggle("active", willOpen);
    });
  });

  /* ---------------- Roadmap: accordion + status filters ---------------- */
  document.querySelectorAll(".phase-head").forEach((head) => {
    head.addEventListener("click", () => {
      head.closest(".phase")?.classList.toggle("open");
    });
  });

  const filterBar = document.querySelector(".roadmap-filters");
  if (filterBar) {
    const phases = document.querySelectorAll(".phase");
    filterBar.querySelectorAll(".filter-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        filterBar.querySelectorAll(".filter-btn").forEach((b) => b.classList.remove("active"));
        btn.classList.add("active");
        const filter = btn.dataset.filter;
        phases.forEach((p) => {
          const match = filter === "all" || p.classList.contains(filter);
          p.style.display = match ? "" : "none";
        });
      });
    });
  }

  /* ---------------- Footer year ---------------- */
  document.querySelectorAll("[data-year]").forEach((el) => (el.textContent = new Date().getFullYear()));
})();
