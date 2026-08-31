This is a piece of software designed to extract, and eventually inject information to and from `.pac` files in Super Smash Brothers Brawl.

Currently, it is possible to use this with command line to get information from the PSA files in human-readable format.

For example, on windows:
```
.\psax.exe FitMario.pac --list-subactions --with-body > MarioPac.txt
```

This command will write all sub-actions used by that Mario file into a text file. You could use this for version control for example.

Other useful commands for the executable:
```
--subaction <id> [tab]            Decode events for a subaction. Tab can be [main,gfx,sfx,other], defaults to all four.
--events <hex-offset>             Decode events at a MISC stored offset. Useful for subroutine inspection.
--audit-sfx [--min N] [--max N]   Find all uses of SFX-relevant events across all subactions. Includes hitsfx. You can gate with minimums and maximums to audit if any PSA uses offsets outside the expected range for the character.
---list-subroutines [--with-body] Discover every subroutine reachable from every SubAction and print callers. Your other option is to do this manually in PSACompressor.
--audit-var                       List any getter or setter for any variable across all subactions/subroutines. Helpful for finding what variable is used where.
--find-var <descriptor>           Similar to --audit-var but for a specified variable. e.g. "RA-Basic[8]" or "0x20000008".

Legend: <required input> [optional input]
```

My roadmap includes actions and tables support, writing back to PSA files and ignoring comments, and correctly making injections into Fighter.pac
