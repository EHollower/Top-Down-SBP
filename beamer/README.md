# Top-Down SBP Presentation

30-minute academic presentation on "Top-Down Stochastic Block Partitioning: Turning Graph Clustering Upside Down" (HPDC '25)

## Quick Start

```bash
# Compile the presentation
cd beamer
make

# View the PDF
make view

# Or open directly
xdg-open bin/presentation.pdf
```

## Contents

**Total: 33 slides (30-minute presentation)**

### Structure

1. **Motivation (2 slides)**
   - Why this paper matters
   - The front-loading problem in Bottom-Up SBP

2. **Contributions (3 slides)**
   - What existed before
   - Novel architectural shift to Top-Down
   - Performance highlights (7.7× speedup, 4.1× memory, 403× distributed)

3. **Methods - Background (4 slides)**
   - Graph clustering basics
   - Stochastic Block Model (SBM)
   - Minimum Description Length (MDL)
   - Bottom-Up SBP challenges

4. **Methods - Top-Down SBP (6 slides)**
   - Key architectural shift
   - Block splitting strategy
   - Splitting methods comparison
   - Connectivity Snowball algorithm
   - Algorithm diagrams

5. **Methods - Acceleration (4 slides)**
   - Parallelization with OpenMP
   - Distributed computing with MPI
   - Sampling (SamBaS)
   - Limitations

6. **Results (2 slides)**
   - Experimental setup
   - Performance comparison tables

7. **Code Implementation (4 slides - BONUS)**
   - Implementation overview
   - Connectivity Snowball code
   - Custom Bottom-Up parallelization
   - Benchmark results

8. **Personal Contributions (5 slides)**
   - What I implemented (aggressive Bottom-Up parallelization)
   - Trade-offs discovered (speed vs accuracy)
   - Improvements I would make
   - Potential extensions
   - Lessons learned

9. **Conclusion (2 slides)**
   - Summary
   - Questions & references

## Features

- **Warsaw theme** (academic classic)
- **Moderate mathematical detail** (key formulas with explanations)
- **Paper figures included** (6 screenshots from paper)
- **Syntax-highlighted code** (C++ with OpenMP)
- **Performance tables & comparisons**

## Files

```
beamer/
├── presentation.tex          # Main LaTeX source
├── bin/
│   └── presentation.pdf      # Compiled PDF (917 KB, 33 pages)
├── images/                   # Paper figures
│   ├── Screenshot from 2026-01-05 17-16-31.png  # SBM diagram
│   ├── Screenshot from 2026-01-06 18-52-46.png  # Splitting comparison
│   ├── Screenshot from 2026-01-06 18-54-14.png  # Performance results
│   ├── Screenshot from 2026-01-06 19-03-59.png  # Algorithm phases
│   ├── Screenshot from 2026-01-07 18-13-37.png  # Parallel code
│   └── Screenshot from 2026-01-07 18-28-44.png  # Datasets
├── Makefile                  # Build automation
└── README.md                 # This file
```

## Editing

To customize the presentation:

1. Open `presentation.tex` in your LaTeX editor
2. Modify content as needed
3. Recompile: `make`

### Key Sections to Personalize

- **Line 25:** Add your name (currently "Your Name")
- **Slide 21-24:** Code walkthrough (shows YOUR implementation)
- **Slide 25-29:** Personal contributions (YOUR ideas)
- **Slide 32:** GitHub URL (optional)

## Requirements

- **pdflatex** (TeX Live or MikTeX)
- **Beamer** package
- **Standard LaTeX packages:** graphicx, amsmath, listings, tikz, booktabs

### Installation (Ubuntu/Debian)

```bash
sudo apt-get install texlive-latex-base texlive-latex-extra
```

## Presentation Tips

- **Timing:** ~1 minute per slide (some slides faster/slower)
- **Code slides:** Walk through key lines, don't read everything
- **Figures:** Let audience absorb for 3-5 seconds before explaining
- **Personal contributions:** This is where you differentiate yourself!

## Grading Rubric (from requirements)

- **Up to 8 points:** Good explanation of paper
- **8-10 points:** Code walkthrough + experiments/modifications
  - ✓ Implementation overview (Slide 21)
  - ✓ Code snippets with explanation (Slides 22-23)
  - ✓ Benchmark results (Slide 24)
  - ✓ Custom parallelization (your contribution!)

## Next Steps

1. **Practice:** Rehearse out loud (aim for 28-30 minutes)
2. **Personalize:** Replace "Your Name" and add GitHub link
3. **Optional:** Run actual code demo during presentation
4. **Prepare for Q&A:** Anticipate questions on:
   - Why Top-Down beats Bottom-Up
   - Limitations (when Bottom-Up is better)
   - Your parallelization choices
   - Real-world applications

---

**Good luck with your presentation!** 🎉
