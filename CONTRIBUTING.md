# Contributing to Custom Task Manager

Thank you for considering contributing to Custom Task Manager! Your help would make my life easier... probably.

## Contribution Guidelines

### Coding Style
To maintain consistency, all contributions must follow the existing code style in `CTMBackend`. Ensure that your code aligns with these conventions (probably did not even cover all of it but yeah):
#### **General Formatting**
- Use **4 spaces for indentation** (no tabs).
- Keep all member variables **aligned for readability**, especially in structs (atleast try to, wherever possible).
- **Group members** in classes using multiple access specifier sections with meaningful labels (using //Comment).
- When using `#include`, try to use comments to group each `include` into its own section (check any header file to understand it better).
- Always prefer `std::unordered_map` over `std::map` (even for set, use `std::unordered_set`) unless ordering is necessary.

#### **Naming Conventions**
- **Classes, Structs, Enums & Functions**: Use `PascalCase` (e.g., `CTMProcessScreen`, `ProcessInfo`, `VeryCoolFunction()`).
- **Variables**: Use `camelCase` (e.g., `processId`, `updateProcessInfo`).
- **Boolean Variables**: Use `is` or `can` prefixes (atleast try to, not that big of a deal but yeah) (e.g., `isStaleEntry`, `canTerminate`).
- **Constants & Global Variables**: Use `UPPER_CASE_WITH_UNDERSCORES` if necessary.

#### **Structs & Classes**
- **Structs should have all members aligned for 8-byte alignment** (i forgot to do it for classes so if you could... pls fix it).
- **Use constructor initialization lists** for member initialization.

#### **Type Aliases**
- Use `using` instead of `typedef` for type aliases (if you feel like c++ syntax hurts your brain) (e.g., `using ProcessInfoVector = std::vector<ProcessInfo>;`).

#### **Comments**
- **Use `//` for single-line comments** and `/* */` for multi-line comments.
- **Explain complex logic** and key parts of the code.
- **Avoid redundant comments**—code should be self-explanatory (unless you want to do some '**tomfoolery**').
- **Don't add spacing after `//`**.

#### **Function Declarations & Definitions**
- **Use meaningful function names** and avoid abbreviations unless widely recognized.
- **Separate different groups of functions** with meaningful `private:` or `protected:` labels.
- **Group related helper functions together**.

### Commit Messages
- Write meaningful commit messages that accurately describe the changes made (a bit of '**tomfoolery**' is allowed).

### Pull Requests
- Ensure your code follows all the above guidelines before submitting.
- Provide a **clear description** of the changes in your PR.
- Reference any related issues in the pull request description.

### Testing
- Test your code thoroughly to ensure it does not break existing functionality (and your own functionality).
- Test edge cases and performance considerations where applicable (this reminds me of my bad code... anyways).

### Finally...
If you reached till here without getting bored... i have nothing to say except thank you for reading this boring ahhh `.md`.
Good luck contributing (if you are) to this horrible mess of a code.
