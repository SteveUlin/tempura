# Statistical Rethinking 2025

C++ implementations of homework problems from Richard McElreath's Statistical Rethinking course.

**Course materials:** https://github.com/rmcelreath/stat_rethinking_2025

## Datasets

Datasets are sourced from the `rethinking` R package:
https://github.com/rmcelreath/rethinking/tree/master/data

### bangladesh.csv

Contraceptive use data from 1934 Bangladeshi women (1988 Bangladesh Fertility Survey).

**Source:** https://raw.githubusercontent.com/rmcelreath/rethinking/master/data/bangladesh.csv

**Download:**
```bash
curl -sL "https://raw.githubusercontent.com/rmcelreath/rethinking/master/data/bangladesh.csv" -o bangladesh.csv
sed -i 's/;/,/g' bangladesh.csv  # convert semicolons to commas
```

**Columns:**
| Column | Type | Description |
|--------|------|-------------|
| woman | int | Unique ID (1-1934) |
| district | int | District ID (1-61) |
| use.contraception | int | 0=no, 1=yes |
| living.children | int | 1-4 (4 means "3+") |
| age.centered | double | Age minus mean |
| urban | int | 0=rural, 1=urban |

**Reference:** Huq, N. M., and Cleland, J. 1990. Bangladesh Fertility Survey 1989 (Main Report). Dhaka: National Institute of Population Research and Training.

## Weekly Topics

| Week | Lectures |
|------|----------|
| 1 | Science Before Statistics / Garden of Forking Data |
| 2 | Geocentric Models / Categories and Curves |
| 3 | Elemental Confounds / Good and Bad Controls |
| 4 | Overfitting / MCMC |
| 5 | Modeling Events / Counts and Confounds |
| 6 | Ordered Categories / Multilevel Models |
| 7 | Multilevel Adventures / Correlated Features |
| 8 | Social Networks / Gaussian Processes |
| 9 | Measurement / Missing Data |
