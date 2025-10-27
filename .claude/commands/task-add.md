# Add New Task

Parse natural language input to create tasks intelligently with user verification before adding.

## Workflow

### Step 1: Parse Request

Extract key information:
- **What**: Specific items or general request
- **Component**: bayes, symbolic3, matrix2, optimization, quadrature, meta, autodiff
- **Priority**: critical, high, medium (default), low
- **Quantity**: Single vs multiple tasks

### Step 2: Gather Context (Optional)

When helpful:
- Search codebase with Glob/Grep to understand existing code
- Use WebSearch/WebFetch for algorithmic details or best practices
- Present findings to user before generating proposals

### Step 3: Generate Task Proposals

**IMPORTANT: Generate proposals only - do NOT create tasks yet!**

For specific requests: Generate 1 task matching specification
For general requests: Generate 4-6 related tasks using domain knowledge
For list requests: Generate one task per item

Infer smart defaults:
- Component from keywords (e.g., "distribution" → bayes)
- Priority from urgency keywords (default: medium)
- Tags from domain (e.g., distributions → ["distribution", "statistics"])
- Description with implementation details
- Acceptance criteria (tests, constexpr support, documentation)

### Step 4: Present for Verification

**CRITICAL: Always get user approval before creating!**

Show proposals clearly:
```
I've generated these task proposals:

**Task 1: Add Chi-squared distribution to bayes/**
- Priority: medium
- Tags: distribution, statistics
- Description: Implement Chi-squared(k) distribution with PDF, CDF...
- Acceptance:
  - PDF and CDF implementation
  - Unit tests with reference values
  - constexpr support where applicable

**Task 2: Add Student-t distribution to bayes/**
...

Would you like me to:
1. Create all these tasks
2. Create only some (specify which)
3. Modify before creating
4. Cancel
```

Wait for user confirmation.

### Step 5: Create Approved Tasks

Use the programmatic task script:

```bash
./scripts/task add \
  --title "Add Poisson distribution to bayes/" \
  --component bayes \
  --description "Implement Poisson(λ) distribution:
- PMF, CDF, quantile functions
- Parameter estimation (MLE)
- Unit tests with reference values
- Integration with RNG infrastructure" \
  --priority medium \
  --tags "distribution,discrete,counting" \
  --acceptance "PMF and CDF implementation" \
  --acceptance "Unit tests with reference values" \
  --acceptance "constexpr support where applicable" \
  --notes "Useful for count data modeling"
```

For multiple tasks, run the command multiple times.

### Step 6: Confirm Creation

Show summary:
```
✓ Created 5 tasks:
  #4 [MEDIUM] Add Chi-squared distribution to bayes/
  #5 [MEDIUM] Add Student-t distribution to bayes/
  ...

Run `./scripts/task list` or `/task-next` to start working!
```

## Example: Multiple Related Tasks

```
User: /task-add add a few more distributions to bayes

AI: Let me check what distributions already exist...
[Uses Glob to scan src/bayes/*.h]

Found: Normal, Uniform, Bernoulli, Beta, Cauchy, Exponential, Gamma, Binomial, Logistic

I can propose these complementary distributions:
1. Chi-squared (special case of Gamma)
2. Student-t (hypothesis testing)
3. F-distribution (ANOVA)
4. Pareto (heavy-tailed)
5. Lognormal (finance)
6. Poisson (count data)

Proceed with task proposals?

User: Yes, skip Poisson

AI: [Shows 5 detailed proposals]

Would you like me to:
1. Create all these tasks
...

User: Create all

AI: [Uses ./scripts/task add for each]

✓ Created 5 tasks (#4-#8)
```

## Example: Single Specific Task

```
User: /task-add implement Pareto distribution [high priority]

AI: [Shows 1 detailed proposal]

User: Yes

AI: [Uses ./scripts/task add]

✓ Created task #4 [HIGH] Add Pareto distribution to bayes/
```

## Task Structure

```json
{
  "id": <auto>,
  "title": "Task title",
  "description": "Detailed description",
  "component": "bayes|symbolic3|matrix2|...",
  "priority": "low|medium|high|critical",
  "tags": ["tag1", "tag2"],
  "status": "pending",
  "created": "YYYY-MM-DD",
  "started": null,
  "notes": "Optional notes",
  "acceptance": ["criterion1", "criterion2"]
}
```

**Key Points:**
- Always verify with user before creating tasks
- Use today's date (YYYY-MM-DD)
- Validate component and priority values
- Generate helpful descriptions and acceptance criteria
