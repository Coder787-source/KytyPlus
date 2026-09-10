# Compatibility List

> 📢 **CALL FOR HARDWARE TESTERS:** Because the maintainer does not personally own a physical PlayStation 5 console, community validation is critical. If you own a jailbroken console and want to help us push past the *Dead Cells* menu milestone using your legally obtained backups, please submit your boot logs and game footage! You can submit an automated format instantly via our **[New Compatibility Report Form](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)**.


Community-reported results for KytyPlus. Entries here come **only** from user-submitted
[Compatibility report](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)
issues — nobody on this project claims to have personally verified every row.

> [!CAUTION]
> "Boots further" is not "playable." This table tracks how far a title gets, not whether it's
> recommended to play. See the [README](../README.md) for what KytyPlus is / is not.

## Status definitions

These match the dropdown in the compatibility report template, so a submitted issue maps
directly onto a row here.

| Status | Meaning |
|---|---|
| Does not boot | Crashes or hangs before any video output |
| Boots / logos only | Splash/logo screens render, nothing playable reached |
| Reaches menu | Title screen or main menu is reachable |
| Ingame (broken) | Gameplay starts but is broken (crashes, corrupt visuals, stuck) |
| Ingame (playable-ish) | Gameplay is reachable and roughly functions, not a quality guarantee |
| Playable | Gameplay is reachable and holds up well enough to actually play through |
| Other | Doesn't fit the above — see notes column |

## How this list is maintained

1. Testers file a [Compatibility report](https://github.com/Coder787-source/KytyPlus/issues/new?template=compatibility.yml)
   issue for a **legally obtained** title.
2. A maintainer reviews the report (sanity-checks the log/version, no dump/firmware requests) and
   closes the issue as **completed**. Closing as completed is the approval step — a bot
   (`.github/workflows/compat-table-update.yml`)
   then parses the issue form and commits the row below automatically, so the table can't drift
   out of sync with approved issues. Close as **not planned** instead if a report should not be
   added (spam, dump/firmware request, insufficient info).
3. If a newer report contradicts an older row for the same title, the newer report wins — the bot
   updates Status/Version in place and appends the new issue link to the Reports column so history
   isn't lost.
4. Rows are sorted alphabetically by title automatically. Keep one row per title; the bot matches
   existing rows by Title ID first, falling back to title text.

Filing accurate reports for titles you already own is the
single most useful contribution you can make here — see
[CONTRIBUTING.md](../CONTRIBUTING.md) for details.

---

## Native (PS5)

Titles running on KytyPlus's **native** engine (Vulkan, accountless HLE kernel). This is the
headline compatibility number for the project.

| Title | Title ID | Status | Last tested (version) | Reports |
|---|---|---|---|---|
| Crash Bandicoot 4: It's About Time | PPSA02433 | Boots / logos only | v3.3 ([@Crispy81](https://github.com/KytyPS5/KytyPS5/issues/88) tester) | [#88](https://github.com/KytyPS5/KytyPS5/issues/88) |
| Dead Cells | PPSA-15554 | Reaches menu | v1.8 | [#3](https://github.com/Coder787-source/KytyPlus/issues/3) |

<!--
Row template (copy/paste and fill in from the issue). Use a full issue URL, not a relative link:
| Game Name | PPSAxxxxx | Ingame (broken) | v1.3 | [#123](https://github.com/Coder787-source/KytyPlus/issues/123) |
-->

---

## shadPS4 (PS4, via delegation)

Titles that run by delegating to the bundled **shadPS4** backend. These are **not** running on
KytyPlus's native engine — they are PS4 titles executed through the shadPS4 runtime that KytyPlus
launches. This number is **separate** from the native PS5 count and must not be merged with it.

> [!IMPORTANT]
> PS4 titles require the user to provide a **legally dumped `sys_modules`** set. KytyPlus does not
> ship or download firmware. See [README](../README.md) for setup.

Status values below are mapped from the upstream
[shadPS4 compatibility list](https://github.com/shadps4-compatibility/shadps4-game-compatibility)
(playable → Playable, ingame → Ingame (playable-ish), menus → Reaches menu, boots → Boots / logos
only, nothing → Does not boot). Where a title has multiple reports, the best status is shown.

| Title | Title ID | Status | Reports |
|---|---|---|---|
| [PROTOTYPE® BIOHAZARD BUNDLE] | CUSA06545 | Ingame (playable-ish) | [#2190](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2190) |
| 112th Seed | CUSA20485 | Playable | [#1424](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1424) |
| 13 Sentinels: Aegis Rim | CUSA19620 | Playable | [#529](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/529) |
| 2Dark | CUSA04802 | Playable | [#938](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/938) |
| 36 Fragments of Midnight | CUSA10907 | Reaches menu | [#2394](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2394) |
| 8-Bit Armies | CUSA07626 | Playable | [#1899](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1899) |
| A Boy and His Blob | CUSA02700 | Playable | [#1354](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1354) |
| A Hole New World | CUSA08274 | Playable | [#1448](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1448) |
| A KING'S TALE: FINAL FANTASY XV | CUSA05932 | Playable | [#1506](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1506) |
| A Plague Tale: Innocence | CUSA10812 | Boots / logos only | [#931](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/931) |
| A Way Out | CUSA07995 | Reaches menu | [#1856](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1856) |
| A-Train Express | CUSA13691 | Ingame (playable-ish) | [#2005](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2005) |
| ABZU | CUSA03364 | Playable | [#2557](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2557) |
| ACE COMBAT™ 7: SKIES UNKNOWN | CUSA07202 | Ingame (playable-ish) | [#364](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/364) |
| Aces of the Luftwaffe | CUSA01182 | Ingame (playable-ish) | [#644](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/644) |
| AdVenture Capitalist | CUSA03234 | Playable | [#1231](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1231) |
| Adventure Time: Pirates of the Enchiridion | CUSA11071 | Ingame (playable-ish) | [#2781](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2781) |
| AereA | CUSA08005 | Playable | [#2075](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2075) |
| Agents of Mayhem | CUSA03067 | Reaches menu | [#602](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/602) |
| Akai Katana Shin | CUSA40396 | Reaches menu | [#1382](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1382) |
| AKIBA'S BEAT | CUSA07108 | Ingame (playable-ish) | [#1691](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1691) |
| AKIBA'S TRIP: UNDEAD ＆ UNDRESSED | CUSA01166 | Ingame (playable-ish) | [#2212](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2212) |
| Alan Wake Remastered | CUSA24653 | Ingame (playable-ish) | [#2559](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2559) |
| Alekhine's Gun | CUSA02789 | Does not boot | [#2750](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2750) |
| Alien: Isolation™ | CUSA00362 | Ingame (playable-ish) | [#276](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/276) |
| ALIENATION™ | CUSA00062 | Ingame (playable-ish) | [#2415](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2415) |
| All 4 | CUSA00072 | Boots / logos only | [#2350](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2350) |
| Allumette | CUSA05884 | Does not boot | [#1515](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1515) |
| Amnesia Collection | CUSA05457 | Reaches menu | [#2409](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2409) |
| Amnesia Collection | CUSA05882 | Reaches menu | [#1105](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1105) |
| Among Us | CUSA27793 | Does not boot | [#2076](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2076) |
| Amplitude | CUSA02670 | Playable | [#1898](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1898) |
| Angry Birds Star Wars | CUSA00184 | Playable | [#1684](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1684) |
| Angry Birds VR: Isle of Pigs | CUSA15269 | Reaches menu | [#1330](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1330) |
| Another Sight | CUSA15304 | Boots / logos only | [#959](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/959) |
| Another World - 20th Anniversary Edition | CUSA00748 | Playable | [#1857](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1857) |
| anywhereVR | CUSA08643 | Playable | [#1516](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1516) |
| Ape Escape™ 2 | CUSA02269 | Does not boot | [#2349](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2349) |
| Apex Legends | CUSA12552 | Reaches menu | [#2586](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2586) |
| Apex Legends | CUSA12540 | Boots / logos only | [#1517](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1517) |
| Apollo Justice: Ace Attorney Trilogy | CUSA37736 | Ingame (playable-ish) | [#1836](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1836) |
| Apple TV | CUSA24186 | Does not boot | [#1518](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1518) |
| Apsulov: End of Gods | CUSA18472 | Reaches menu | [#2106](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2106) |
| AQUA KITTY - Milk Mine Defender DX | CUSA06225 | Does not boot | [#1358](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1358) |
| Arcade Archives SUNSETRIDERS | CUSA19547 | Playable | [#1858](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1858) |
| ArcaniA : The Complete Tale | CUSA01644 | Ingame (playable-ish) | [#980](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/980) |
| Arizona Sunshine | CUSA07980 | Ingame (playable-ish) | [#2006](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2006) |
| ARK: Survival Evolved | CUSA06782 | Boots / logos only | [#349](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/349) |
| Arkanoid - Eternal Battle | CUSA31975 | Does not boot | [#2513](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2513) |
| Armello | CUSA03287 | Playable | [#2087](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2087) |
| Armello | CUSA03300 | Ingame (playable-ish) | [#981](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/981) |
| ARMORED CORE™ VI FIRES OF RUBICON™ | CUSA25368 | Reaches menu | [#2478](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2478) |
| ARMORED CORE™ VI FIRES OF RUBICON™ | CUSA32599 | Reaches menu | [#2047](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2047) |
| ARMORED CORE™ VI FIRES OF RUBICON™ | CUSA32600 | Reaches menu | [#1197](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1197) |
| ARMORED WARFARE | CUSA07587 | Does not boot | [#1404](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1404) |
| Art of Fighting Anthology | CUSA03754 | Does not boot | [#1435](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1435) |
| art of rally | CUSA27916 | Does not boot | [#2782](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2782) |
| Asphalt Legends | CUSA44116 | Boots / logos only | [#1519](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1519) |
| Assassin's Creed The Ezio Collection | CUSA04893 | Ingame (playable-ish) | [#421](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/421) |
| Assassin's Creed Valhalla | CUSA18534 | Does not boot | [#1195](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1195) |
| Assassin's Creed® III Remastered | CUSA11560 | Ingame (playable-ish) | [#418](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/418) |
| Assassin's Creed® IV Black Flag | CUSA00206 | Does not boot | [#1312](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1312) |
| Assassin's Creed® IV Black Flag | CUSA00009 | Does not boot | [#414](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/414) |
| Assassin's Creed® Odyssey | CUSA09303 | Reaches menu | [#1658](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1658) |
| Assassin's Creed® Origins | CUSA05625 | Reaches menu | [#435](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/435) |
| Assassin's Creed® Rogue Remastered | CUSA10123 | Reaches menu | [#432](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/432) |
| Assassin's Creed® Syndicate | CUSA02377 | Boots / logos only | [#1223](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1223) |
| Assassin's Creed® Unity | CUSA00605 | Reaches menu | [#433](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/433) |
| Assassin's Creed® Unity | CUSA00663 | Reaches menu | [#25](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/25) |
| Astria Ascending | CUSA27170 | Ingame (playable-ish) | [#604](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/604) |
| ASTRO AQUA KITTY | CUSA20578 | Ingame (playable-ish) | [#1377](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1377) |
| ASTRO BOT Rescue Mission | CUSA12431 | Does not boot | [#2601](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2601) |
| AtomicHeart | CUSA37321 | Boots / logos only | [#935](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/935) |
| Attack of the Toy Tanks | CUSA15843 | Playable | [#2496](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2496) |
| Attack on Titan | CUSA03804 | Reaches menu | [#2783](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2783) |
| Attack On Titan 2 | CUSA08956 | Ingame (playable-ish) | [#90](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/90) |
| Autobahn Police Simulator 3 | CUSA30313 | Boots / logos only | [#2386](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2386) |
| BABYLON'S FALL | CUSA29015 | Does not boot | [#2107](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2107) |
| Back 4 Blood | CUSA14275 | Boots / logos only | [#854](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/854) |
| Back to the Future: The Game | CUSA02924 | Playable | [#2784](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2784) |
| Back to the Future: The Game | CUSA02972 | Playable | [#1666](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1666) |
| Baja: Edge of Control HD | CUSA07707 | Reaches menu | [#574](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/574) |
| Balatro | CUSA47498 | Playable | [#2498](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2498) |
| Baldur's Gate and Baldur's Gate II: Enhanced Editions | CUSA15671 | Playable | [#2718](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2718) |
| Bandit Six: Combined Arms | CUSA07978 | Does not boot | [#2187](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2187) |
| Banner Saga 2 | CUSA04444 | Ingame (playable-ish) | [#1895](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1895) |
| Banner Saga 3 | CUSA11054 | Ingame (playable-ish) | [#1896](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1896) |
| Bastion | CUSA02057 | Boots / logos only | [#1821](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1821) |
| Bastion | CUSA02052 | Boots / logos only | [#1429](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1429) |
| Batman: Return to Arkham - Arkham Asylum | CUSA04601 | Boots / logos only | [#2756](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2756) |
| Batman: Return to Arkham - Arkham Asylum | CUSA04607 | Boots / logos only | [#91](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/91) |
| Batman: Return to Arkham - Arkham City | CUSA04516 | Boots / logos only | [#2755](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2755) |
| Batman: Return to Arkham - Arkham City | CUSA02000 | Boots / logos only | [#92](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/92) |
| BATMAN™: ARKHAM KNIGHT | CUSA00135 | Boots / logos only | [#1837](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1837) |
| BATMAN™: ARKHAM KNIGHT | CUSA00133 | Boots / logos only | [#467](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/467) |
| Batman™: Arkham VR | CUSA05335 | Does not boot | [#2785](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2785) |
| Battlefield™ 1 | CUSA02429 | Boots / logos only | [#1049](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1049) |
| Battlefield™ 1 | CUSA02387 | Does not boot | [#1206](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1206) |
| Battlefield™ Hardline | CUSA00625 | Boots / logos only | [#817](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/817) |
| Battlefield™ V | CUSA08724 | Reaches menu | [#960](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/960) |
| Battlezone® | CUSA05174 | Ingame (playable-ish) | [#2008](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2008) |
| Beach Buggy Racing 2: Island Adventure | CUSA26789 | Playable | [#1026](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1026) |
| Beast Quest | CUSA09044 | Ingame (playable-ish) | [#243](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/243) |
| Beat Saber | CUSA12878 | Boots / logos only | [#1164](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1164) |
| beIN Sports | CUSA01258 | Does not boot | [#2534](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2534) |
| Bendy and the Ink Machine | CUSA13637 | Ingame (playable-ish) | [#1859](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1859) |
| Bendy and the Ink Machine | CUSA13635 | Ingame (playable-ish) | [#937](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/937) |
| Beyond: Two Souls™ | CUSA00504 | Ingame (playable-ish) | [#1897](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1897) |
| Beyond: Two Souls™ | CUSA00267 | Ingame (playable-ish) | [#843](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/843) |
| Biomutant | CUSA10041 | Boots / logos only | [#1367](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1367) |
| Biomutant | CUSA09848 | Does not boot | [#1818](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1818) |
| Bioshock Infinite: The Complete Edition | CUSA03980 | Reaches menu | [#2242](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2242) |
| BioShock: The Collection | CUSA03979 | Ingame (playable-ish) | [#2238](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2238) |
| Black Clover: Quartet Knights | CUSA10771 | Reaches menu | [#1986](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1986) |
| BLACKHOLE: Complete Edition | CUSA06921 | Playable | [#2077](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2077) |
| Blacklight: Retribution | CUSA00253 | Reaches menu | [#2778](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2778) |
| Blacksad: Under the Skin | CUSA16064 | Ingame (playable-ish) | [#903](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/903) |
| BLADESTORM: Nightmare | CUSA01919 | Ingame (playable-ish) | [#1796](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1796) |
| Blair Witch | CUSA18142 | Reaches menu | [#958](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/958) |
| BLAZBLUE CROSS TAG BATTLE | CUSA10105 | Playable | [#1325](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1325) |
| BLAZBLUE CROSS TAG BATTLE -Trial Version- | CUSA12480 | Reaches menu | [#2585](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2585) |
| BLEACH Rebirth of Souls | CUSA27765 | Reaches menu | [#1235](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1235) |
| Bleach: Brave Souls | CUSA26679 | Boots / logos only | [#1520](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1520) |
| Blood & Truth | CUSA11098 | Does not boot | [#2188](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2188) |
| Blood & Truth | CUSA11108 | Does not boot | [#2009](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2009) |
| Bloodborne® The Old Hunters Edition | CUSA03014 | Ingame (playable-ish) | [#869](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/869) |
| Bloodborne™ | CUSA03173 | Ingame (playable-ish) | [#2243](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2243) |
| Bloodborne™ | CUSA00207 | Ingame (playable-ish) | [#2046](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2046) |
| Bloodborne™ | CUSA00900 | Ingame (playable-ish) | [#497](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/497) |
| Bloodborne™ The Old Hunters Edition | CUSA03023 | Ingame (playable-ish) | [#211](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/211) |
| Bloodstained: Curse of the Moon | CUSA12562 | Playable | [#1862](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1862) |
| Bloodstained: Curse of the Moon 2 | CUSA23490 | Playable | [#1863](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1863) |
| Bloons TD 5 | CUSA08142 | Playable | [#1521](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1521) |
| Bloons TD 6 | CUSA29953 | Boots / logos only | [#1522](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1522) |
| Blue Fire | CUSA19549 | Reaches menu | [#1374](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1374) |
| Borderlands: The Handsome Collection | CUSA02893 | Reaches menu | [#985](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/985) |
| Borderlands® 2 VR | CUSA13946 | Does not boot | [#1331](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1331) |
| Borderlands® 3 | CUSA08025 | Ingame (playable-ish) | [#1690](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1690) |
| Borderlands® 3 | CUSA07823 | Ingame (playable-ish) | [#245](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/245) |
| Borderlands®: Game of the Year Edition | CUSA10455 | Ingame (playable-ish) | [#2291](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2291) |
| Bouncy Bullets | CUSA14182 | Playable | [#2396](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2396) |
| Bound | CUSA04380 | Does not boot | [#296](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/296) |
| Breach & Clear: Deadline | CUSA03742 | Does not boot | [#1865](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1865) |
| Broken Sword 5 - the Serpent's Curse | CUSA02500 | Playable | [#1207](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1207) |
| Bubsy: The Woolies Strike Back | CUSA09452 | Ingame (playable-ish) | [#1365](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1365) |
| Burnout Paradise Remastered | CUSA10851 | Playable | [#1654](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1654) |
| Burnout Paradise Remastered | CUSA10866 | Boots / logos only | [#2266](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2266) |
| Call of Duty® | CUSA23827 | Does not boot | [#1523](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1523) |
| Call of Duty® Ghosts | CUSA00025 | Boots / logos only | [#2142](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2142) |
| Call of Duty®: Advanced Warfare | CUSA00803 | Ingame (playable-ish) | [#94](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/94) |
| Call of Duty®: Advanced Warfare | CUSA00851 | Does not boot | [#1338](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1338) |
| Call of Duty®: Black Ops 4 | CUSA11100 | Boots / logos only | [#2602](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2602) |
| Call of Duty®: Black Ops 4 | CUSA12443 | Does not boot | [#2615](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2615) |
| Call of Duty®: Black Ops 4 | CUSA12444 | Does not boot | [#2273](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2273) |
| Call of Duty®: Black Ops 4 | CUSA12446 | Does not boot | [#1343](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1343) |
| Call of Duty®: Black Ops Cold War | CUSA24267 | Boots / logos only | [#2277](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2277) |
| Call of Duty®: Black Ops Cold War - Alpha | CUSA20073 | Boots / logos only | [#2535](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2535) |
| Call of Duty®: Black Ops III | CUSA02624 | Boots / logos only | [#2351](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2351) |
| Call of Duty®: Black Ops III | CUSA02628 | Boots / logos only | [#2268](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2268) |
| Call of Duty®: Black Ops III | CUSA02290 | Boots / logos only | [#961](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/961) |
| Call of Duty®: Infinite Warfare | CUSA05294 | Reaches menu | [#2258](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2258) |
| Call of Duty®: Infinite Warfare | CUSA05282 | Reaches menu | [#1854](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1854) |
| Call of Duty®: Infinite Warfare | CUSA04762 | Reaches menu | [#988](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/988) |
| Call of Duty®: Modern Warfare® | CUSA15277 | Reaches menu | [#2536](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2536) |
| Call of Duty®: Modern Warfare® | CUSA08829 | Boots / logos only | [#1087](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1087) |
| Call of Duty®: Modern Warfare® - 2v2 Alpha | CUSA17218 | Boots / logos only | [#2537](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2537) |
| Call of Duty®: Modern Warfare® Remastered | CUSA05379 | Ingame (playable-ish) | [#2475](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2475) |
| Call of Duty®: Vanguard | CUSA24041 | Boots / logos only | [#1176](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1176) |
| Call of Duty®: WWII | CUSA08630 | Reaches menu | [#2352](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2352) |
| Call of Duty®: WWII | CUSA05969 | Reaches menu | [#954](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/954) |
| CAPCOM FIGHTING COLLECTION | CUSA31205 | Playable | [#1426](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1426) |
| Cars 3: Driven to Win | CUSA07044 | Ingame (playable-ish) | [#2638](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2638) |
| Cars 3: Driven to Win | CUSA07027 | Ingame (playable-ish) | [#709](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/709) |
| Castlevania Advance Collection | CUSA28029 | Ingame (playable-ish) | [#1866](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1866) |
| Castlevania Anniversary Collection | CUSA15109 | Playable | [#1867](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1867) |
| Castlevania Requiem: Symphony Of The Night & Rondo Of Blood | CUSA13434 | Ingame (playable-ish) | [#1455](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1455) |
| Catherine: Full Body | CUSA15036 | Playable | [#530](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/530) |
| Catherine: Full Body | CUSA14836 | Ingame (playable-ish) | [#858](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/858) |
| Celeste | CUSA11235 | Playable | [#2522](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2522) |
| CHAOS;CHILD らぶchu☆chu!! | CUSA07047 | Playable | [#1390](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1390) |
| Chicken Police | CUSA20204 | Ingame (playable-ish) | [#914](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/914) |
| CHRONO CROSS: THE RADICAL DREAMERS EDITION | CUSA23909 | Playable | [#2560](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2560) |
| Cities: Skylines | CUSA06548 | Playable | [#1524](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1524) |
| Cladun Returns: This is Sengoku! | CUSA06770 | Playable | [#772](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/772) |
| Clicker Heroes | CUSA05209 | Playable | [#1525](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1525) |
| Clockwork Aquario | CUSA29915 | Playable | [#1381](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1381) |
| Clockwork Aquario | CUSA26908 | Playable | [#864](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/864) |
| CODE VEIN | CUSA10410 | Ingame (playable-ish) | [#1526](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1526) |
| COGEN: Sword of Rewind | CUSA20571 | Does not boot | [#2211](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2211) |
| Colossal Cave | CUSA41246 | Ingame (playable-ish) | [#990](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/990) |
| Conception PLUS: Maidens of the Twelve Stars | CUSA15055 | Does not boot | [#2699](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2699) |
| Concrete Genie | CUSA11875 | Ingame (playable-ish) | [#439](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/439) |
| Concrete Genie | CUSA11834 | Ingame (playable-ish) | [#336](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/336) |
| CONTRA: ROGUE CORPS | CUSA15024 | Playable | [#2764](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2764) |
| Contrast | CUSA00011 | Playable | [#1868](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1868) |
| Control | CUSA11454 | Reaches menu | [#933](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/933) |
| Control Ultimate Edition | CUSA24473 | Reaches menu | [#1102](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1102) |
| Cooking Mama: Cookstar | CUSA25355 | Ingame (playable-ish) | [#95](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/95) |
| Cosmic Star Heroine | CUSA08322 | Playable | [#2562](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2562) |
| Costume Quest 2 | CUSA01202 | Playable | [#1869](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1869) |
| Crash Bandicoot N. Sane Trilogy | CUSA07399 | Playable | [#1655](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1655) |
| Crash Bandicoot N. Sane Trilogy | CUSA07402 | Playable | [#763](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/763) |
| Crash Bandicoot™ 4: It's About Time | CUSA19035 | Ingame (playable-ish) | [#773](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/773) |
| Crash Bandicoot™ 4: It's About Time | CUSA23470 | Reaches menu | [#1005](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1005) |
| Crash™ Team Racing Nitro-Fueled | CUSA13795 | Playable | [#96](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/96) |
| Crash™ Team Racing Nitro-Fueled | CUSA14876 | Does not boot | [#2166](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2166) |
| CRISIS CORE –FINAL FANTASY VII– REUNION | CUSA31349 | Reaches menu | [#1797](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1797) |
| Crisis on the Planet of the Apes VR | CUSA10826 | Does not boot | [#1332](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1332) |
| CrisTales | CUSA16450 | Boots / logos only | [#1798](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1798) |
| Crow: The Legend | CUSA14043 | Does not boot | [#1528](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1528) |
| Crunchyroll | CUSA00095 | Does not boot | [#1529](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1529) |
| Crypt of the NecroDancer | CUSA03610 | Playable | [#2825](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2825) |
| Crysis® Remastered | CUSA18659 | Does not boot | [#2092](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2092) |
| Crysis2® Remastered | CUSA18658 | Does not boot | [#2093](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2093) |
| Crysis3® Remastered | CUSA18657 | Does not boot | [#2094](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2094) |
| CRYSTAR | CUSA15257 | Playable | [#2584](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2584) |
| Cuphead | CUSA20469 | Ingame (playable-ish) | [#2367](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2367) |
| Cyber Shadow | CUSA24216 | Playable | [#2179](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2179) |
| Cyberdimension Neptunia: 4 Goddesses Online | CUSA07998 | Ingame (playable-ish) | [#1794](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1794) |
| Cyberpunk 2077 | CUSA18278 | Boots / logos only | [#940](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/940) |
| Cyberpunk 2077 | CUSA16596 | Boots / logos only | [#97](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/97) |
| Danganronpa 1.2 RELOAD | CUSA06808 | Ingame (playable-ish) | [#1530](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1530) |
| DanMachi BATTLE CHRONICLE | CUSA46310 | Boots / logos only | [#1296](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1296) |
| Dark Rose Valkyrie | CUSA06927 | Reaches menu | [#1578](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1578) |
| DARK SOULS Ⅱ SCHOLAR OF THE FIRST SIN | CUSA01576 | Boots / logos only | [#473](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/473) |
| DARK SOULS III NETWORK STRESS TEST VER. | CUSA03387 | Ingame (playable-ish) | [#1672](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1672) |
| DARK SOULS III　THE FIRE FADES EDITION | CUSA07339 | Ingame (playable-ish) | [#870](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/870) |
| DARK SOULS™ II: Scholar of the First Sin | CUSA01589 | Ingame (playable-ish) | [#2050](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2050) |
| DARK SOULS™ II: Scholar of the First Sin | CUSA01760 | Ingame (playable-ish) | [#1870](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1870) |
| DARK SOULS™ III | CUSA03388 | Ingame (playable-ish) | [#2613](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2613) |
| DARK SOULS™ III | CUSA03365 | Ingame (playable-ish) | [#2538](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2538) |
| DARK SOULS™ III: The Fire Fades™ Edition | CUSA07439 | Ingame (playable-ish) | [#2051](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2051) |
| DARK SOULS™ III: The Fire Fades™ Edition | CUSA08155 | Ingame (playable-ish) | [#374](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/374) |
| DARK SOULS™: REMASTERED | CUSA08495 | Playable | [#2048](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2048) |
| DARK SOULS™: REMASTERED | CUSA08692 | Ingame (playable-ish) | [#1872](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1872) |
| Darkest Dungeon | CUSA11767 | Playable | [#1873](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1873) |
| Darksiders Genesis | CUSA12988 | Boots / logos only | [#1371](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1371) |
| Darksiders II Deathinitive Edition | CUSA02419 | Ingame (playable-ish) | [#2583](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2583) |
| Darksiders III | CUSA08798 | Ingame (playable-ish) | [#1705](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1705) |
| Darksiders III | CUSA08880 | Boots / logos only | [#1364](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1364) |
| Dauntless | CUSA15433 | Reaches menu | [#1405](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1405) |
| Day of the Tentacle Remastered | CUSA01959 | Ingame (playable-ish) | [#867](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/867) |
| DAYS GONE | CUSA09176 | Reaches menu | [#874](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/874) |
| DAYS GONE | CUSA08966 | Boots / logos only | [#1874](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1874) |
| DAZN | CUSA09505 | Does not boot | [#1532](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1532) |
| de Blob | CUSA09934 | Reaches menu | [#2262](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2262) |
| de Blob 2 | CUSA10447 | Does not boot | [#2265](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2265) |
| Dead Alliance | CUSA07756 | Reaches menu | [#2086](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2086) |
| Dead Cells | CUSA11253 | Playable | [#2603](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2603) |
| Dead Island - Definitive Edition | CUSA03291 | Ingame (playable-ish) | [#115](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/115) |
| Dead Nation™: Apocalypse Edition | CUSA00176 | Does not boot | [#2365](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2365) |
| DEAD OR ALIVE 5 Last Round | CUSA01627 | Playable | [#1088](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1088) |
| DEAD OR ALIVE 5 Last Round | CUSA01778 | Playable | [#932](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/932) |
| DEAD OR ALIVE 6 | CUSA12153 | Ingame (playable-ish) | [#1205](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1205) |
| DEAD OR ALIVE Xtreme 3 Fortune | CUSA04555 | Ingame (playable-ish) | [#1309](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1309) |
| DEAD RISING | CUSA04513 | Ingame (playable-ish) | [#247](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/247) |
| Dead Rising 2 | CUSA04313 | Boots / logos only | [#263](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/263) |
| Dead Rising 4: Frank's Big Package | CUSA08558 | Boots / logos only | [#934](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/934) |
| Deadpool | CUSA03245 | Ingame (playable-ish) | [#993](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/993) |
| DEATH STRANDING | CUSA11260 | Reaches menu | [#774](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/774) |
| DEATH STRANDING: Timefall - Behind the Scenes Making Of Digital Video | CUSA17264 | Ingame (playable-ish) | [#2539](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2539) |
| DEATH STRANDING™ | CUSA12605 | Reaches menu | [#2540](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2540) |
| DEATH STRANDING™ | CUSA12606 | Reaches menu | [#361](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/361) |
| Déraciné™ | CUSA11919 | Does not boot | [#2013](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2013) |
| Destiny | CUSA00568 | Reaches menu | [#155](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/155) |
| Destiny | CUSA00219 | Does not boot | [#994](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/994) |
| Destiny 2 | CUSA05042 | Reaches menu | [#1178](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1178) |
| Destiny 2 Beta | CUSA08415 | Reaches menu | [#311](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/311) |
| Destiny Connect: Tick-Tock Travelers | CUSA15534 | Reaches menu | [#1423](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1423) |
| Destiny First Look Alpha | CUSA00763 | Does not boot | [#2141](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2141) |
| Detroit: Become Human™ | CUSA08308 | Boots / logos only | [#1201](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1201) |
| Detroit: Become Human™ | CUSA10345 | Does not boot | [#1406](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1406) |
| Detroit: Become Human™ | CUSA08344 | Does not boot | [#782](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/782) |
| Deus Ex: Mankind Divided™ | CUSA01836 | Reaches menu | [#608](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/608) |
| Devil May Cry 4 Special Edition | CUSA01599 | Ingame (playable-ish) | [#2696](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2696) |
| Devil May Cry 4 Special Edition | CUSA01708 | Boots / logos only | [#2582](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2582) |
| Devil May Cry 5 | CUSA08216 | Reaches menu | [#2706](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2706) |
| Devil May Cry 5 | CUSA08161 | Reaches menu | [#1683](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1683) |
| Devil May Cry 5 Demo | CUSA13447 | Boots / logos only | [#2581](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2581) |
| Devil May Cry® HD Collection | CUSA09407 | Ingame (playable-ish) | [#2717](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2717) |
| Devil May Cry™ HD Collection | CUSA09263 | Ingame (playable-ish) | [#1681](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1681) |
| DEXED™ | CUSA06883 | Does not boot | [#2182](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2182) |
| Diablo III: Reaper of Souls – Ultimate Evil Edition | CUSA00242 | Playable | [#767](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/767) |
| Diablo III: Reaper of Souls – Ultimate Evil Edition | CUSA00433 | Reaches menu | [#879](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/879) |
| DIGIMON STORY CYBER SLEUTH | CUSA02966 | Playable | [#98](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/98) |
| Digimon Story: Cyber Sleuth - Hacker's Memory | CUSA09977 | Playable | [#99](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/99) |
| DIGIMON SURVIVE | CUSA18223 | Ingame (playable-ish) | [#36](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/36) |
| Digimon World: Next Order | CUSA05469 | Ingame (playable-ish) | [#101](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/101) |
| Dishonored 2 | CUSA03603 | Ingame (playable-ish) | [#609](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/609) |
| Disney Classic Games: Aladdin and The Lion King | CUSA16968 | Ingame (playable-ish) | [#1876](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1876) |
| Disney+ | CUSA15607 | Boots / logos only | [#1533](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1533) |
| DISSIDIA FINAL FANTASY NT | CUSA09244 | Ingame (playable-ish) | [#610](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/610) |
| DISSIDIA FINAL FANTASY NT Free Edition | CUSA14210 | Reaches menu | [#1534](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1534) |
| DJMAX RESPECT | CUSA08621 | Ingame (playable-ish) | [#1678](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1678) |
| DmC Devil May Cry™: Definitive Edition | CUSA01013 | Playable | [#1877](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1877) |
| DmC Devil May Cry™: Definitive Edition | CUSA01022 | Ingame (playable-ish) | [#2580](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2580) |
| DOOM | CUSA02092 | Reaches menu | [#611](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/611) |
| DOOM | CUSA02085 | Boots / logos only | [#1353](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1353) |
| Doom 3 VR | CUSA23402 | Does not boot | [#2010](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2010) |
| DOOM Eternal | CUSA13338 | Boots / logos only | [#1535](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1535) |
| DOOM Eternal | CUSA13275 | Boots / logos only | [#612](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/612) |
| DOOM VFR | CUSA09125 | Does not boot | [#2767](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2767) |
| Dragon Age™: Inquisition | CUSA00503 | Ingame (playable-ish) | [#613](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/613) |
| Dragon Age™: Inquisition | CUSA00220 | Reaches menu | [#1928](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1928) |
| DRAGON BALL FighterZ | CUSA09072 | Ingame (playable-ish) | [#102](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/102) |
| DRAGON BALL XENOVERSE | CUSA01341 | Playable | [#103](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/103) |
| DRAGON BALL XENOVERSE 2 | CUSA05350 | Ingame (playable-ish) | [#104](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/104) |
| DRAGON BALL XENOVERSE 2 | CUSA05088 | Ingame (playable-ish) | [#78](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/78) |
| DRAGON BALL Z: KAKAROT | CUSA14655 | Ingame (playable-ish) | [#105](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/105) |
| Dragon Fantasy: Volumes of Westeria | CUSA08156 | Boots / logos only | [#1447](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1447) |
| DRAGON QUEST BUILDERS | CUSA05235 | Playable | [#1093](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1093) |
| DRAGON QUEST HEROES II | CUSA06769 | Ingame (playable-ish) | [#764](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/764) |
| DRAGON QUEST HEROES: The World Tree's Woe and the Blight Below | CUSA02694 | Ingame (playable-ish) | [#519](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/519) |
| DRAGON QUEST XI: Echoes of an Elusive Age | CUSA08518 | Ingame (playable-ish) | [#518](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/518) |
| DRAGON QUEST XI:Echoes of an Elusive Age | CUSA08546 | Reaches menu | [#1363](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1363) |
| Dragon's Crown Pro | CUSA10454 | Playable | [#1451](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1451) |
| Dragon's Crown Pro | CUSA10487 | Ingame (playable-ish) | [#865](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/865) |
| Dragon's Lair Trilogy | CUSA06885 | Does not boot | [#1445](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1445) |
| Dreadnought | CUSA06361 | Reaches menu | [#2421](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2421) |
| Dreams™ | CUSA08010 | Does not boot | [#2812](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2812) |
| Dreams™ | CUSA04301 | Does not boot | [#474](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/474) |
| DreamWorks All-Star Kart Racing | CUSA42499 | Ingame (playable-ish) | [#2662](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2662) |
| DRIVECLUB™ | CUSA00093 | Ingame (playable-ish) | [#1536](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1536) |
| DRIVECLUB™ | CUSA00003 | Ingame (playable-ish) | [#710](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/710) |
| DRIVECLUB™ | CUSA00066 | Does not boot | [#1308](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1308) |
| DRIVECLUB™ VR | CUSA04779 | Boots / logos only | [#2470](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2470) |
| Dungeons 2 | CUSA04056 | Reaches menu | [#2517](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2517) |
| Dungeons 3 | CUSA07824 | Ingame (playable-ish) | [#2474](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2474) |
| Dusk | CUSA32304 | Playable | [#2637](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2637) |
| DYNASTY WARRIORS: Godseekers | CUSA06586 | Ingame (playable-ish) | [#2361](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2361) |
| DYSMANTLE | CUSA15128 | Playable | [#1724](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1724) |
| EA SPORTS™ UFC® | CUSA00264 | Ingame (playable-ish) | [#531](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/531) |
| EA SPORTS™ UFC® | CUSA00222 | Ingame (playable-ish) | [#403](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/403) |
| EA SPORTS™ UFC® 2 | CUSA01968 | Reaches menu | [#859](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/859) |
| EA SPORTS™ UFC® 2 | CUSA01936 | Reaches menu | [#532](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/532) |
| EA SPORTS™ UFC® 3 | CUSA06534 | Reaches menu | [#2541](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2541) |
| EA SPORTS™ UFC® 3 | CUSA06536 | Reaches menu | [#436](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/436) |
| EA SPORTS™ UFC® 4 | CUSA14204 | Ingame (playable-ish) | [#422](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/422) |
| EA SPORTS™ UFC® 4 | CUSA14209 | Ingame (playable-ish) | [#116](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/116) |
| Eagle Flight | CUSA04900 | Boots / logos only | [#2786](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2786) |
| Earth Defense Force 4.1: The Shadow of New Despair | CUSA03131 | Does not boot | [#1188](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1188) |
| EARTH DEFENSE FORCE 5 | CUSA12546 | Does not boot | [#1310](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1310) |
| Edge Of Eternity | CUSA25278 | Ingame (playable-ish) | [#1379](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1379) |
| eFootball 2022 | CUSA26996 | Reaches menu | [#910](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/910) |
| ELDEN RING NIGHTREIGN | CUSA50617 | Reaches menu | [#2384](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2384) |
| ELDEN RING™ | CUSA28863 | Ingame (playable-ish) | [#1540](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1540) |
| ELDEN RING™ Network Test Ver. | CUSA18880 | Ingame (playable-ish) | [#1673](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1673) |
| End of Eternity™ 4K/HD Edition | CUSA17113 | Reaches menu | [#2700](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2700) |
| Enter the Gungeon | CUSA01608 | Ingame (playable-ish) | [#1428](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1428) |
| ESPN | CUSA05214 | Boots / logos only | [#2140](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2140) |
| EVE: Valkyrie  –  Warzone™ | CUSA05789 | Reaches menu | [#2787](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2787) |
| Everybody's Gone To The Rapture™ | CUSA02405 | Does not boot | [#1879](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1879) |
| Evil Dead: The Game | CUSA27708 | Boots / logos only | [#2348](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2348) |
| Evolve | CUSA00432 | Does not boot | [#614](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/614) |
| F.I.S.T. | CUSA28373 | Boots / logos only | [#846](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/846) |
| F1® 24 | CUSA44372 | Does not boot | [#1203](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1203) |
| F1™ 2015 | CUSA00013 | Reaches menu | [#1208](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1208) |
| Fade to Silence | CUSA11498 | Does not boot | [#2824](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2824) |
| Fairy Fencer F ADVENT DARK FORCE | CUSA04853 | Reaches menu | [#2698](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2698) |
| Fall Guys | CUSA29236 | Boots / logos only | [#1537](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1537) |
| Fallout 4 | CUSA02962 | Boots / logos only | [#615](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/615) |
| Fallout 4 | CUSA02557 | Does not boot | [#456](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/456) |
| Fallout 76 | CUSA12057 | Reaches menu | [#2334](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2334) |
| Fallout Shelter | CUSA11772 | Playable | [#1538](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1538) |
| Far Cry® 4 | CUSA00462 | Ingame (playable-ish) | [#117](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/117) |
| Far Cry® 4 | CUSA00496 | Reaches menu | [#673](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/673) |
| Far Cry® 5 | CUSA05847 | Does not boot | [#1659](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1659) |
| Far Cry® Primal | CUSA03309 | Ingame (playable-ish) | [#2802](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2802) |
| Farming Simulator 15 | CUSA01565 | Playable | [#370](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/370) |
| Farpoint | CUSA04179 | Does not boot | [#2003](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2003) |
| Fate Extella | CUSA06200 | Boots / logos only | [#2697](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2697) |
| FATED : The Silent Oath | CUSA05000 | Does not boot | [#2183](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2183) |
| FIFA 14 | CUSA00128 | Does not boot | [#1400](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1400) |
| FIFA 15 | CUSA00722 | Ingame (playable-ish) | [#2067](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2067) |
| FIFA 16 | CUSA02126 | Does not boot | [#2835](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2835) |
| FIFA 17 | CUSA03214 | Ingame (playable-ish) | [#2160](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2160) |
| FIFA 18 | CUSA07994 | Ingame (playable-ish) | [#1495](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1495) |
| FIFA 19 | CUSA11608 | Boots / logos only | [#1494](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1494) |
| FIFA 20 | CUSA15545 | Ingame (playable-ish) | [#1498](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1498) |
| FIFA 21 | CUSA19623 | Ingame (playable-ish) | [#2366](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2366) |
| FIFA 22 | CUSA27106 | Ingame (playable-ish) | [#2836](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2836) |
| FINAL FANTASY | CUSA33817 | Playable | [#249](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/249) |
| FINAL FANTASY II | CUSA33821 | Playable | [#250](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/250) |
| FINAL FANTASY III | CUSA33825 | Ingame (playable-ish) | [#163](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/163) |
| FINAL FANTASY IV | CUSA33829 | Reaches menu | [#164](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/164) |
| FINAL FANTASY IX | CUSA08918 | Playable | [#2419](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2419) |
| FINAL FANTASY TYPE-0 HD | CUSA00994 | Playable | [#256](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/256) |
| FINAL FANTASY V | CUSA33833 | Does not boot | [#1083](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1083) |
| FINAL FANTASY VI | CUSA33837 | Does not boot | [#1084](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1084) |
| FINAL FANTASY VII | CUSA01847 | Ingame (playable-ish) | [#2417](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2417) |
| FINAL FANTASY VII REMAKE | CUSA07187 | Ingame (playable-ish) | [#2588](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2588) |
| FINAL FANTASY VII REMAKE | CUSA07211 | Ingame (playable-ish) | [#1539](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1539) |
| FINAL FANTASY VII REMAKE | CUSA07052 | Does not boot | [#2159](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2159) |
| FINAL FANTASY VIII Remastered | CUSA08751 | Playable | [#1880](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1880) |
| FINAL FANTASY X/X-2 HD Remaster | CUSA01244 | Reaches menu | [#2579](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2579) |
| FINAL FANTASY X/X-2 HD Remaster | CUSA01227 | Reaches menu | [#1085](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1085) |
| FINAL FANTASY Ⅻ THE ZODIAC AGE | CUSA05532 | Does not boot | [#2249](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2249) |
| FINAL FANTASY Ⅻ THE ZODIAC AGE | CUSA05531 | Does not boot | [#2173](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2173) |
| FINAL FANTASY XV | CUSA01615 | Reaches menu | [#2443](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2443) |
| FINAL FANTASY XV | CUSA01633 | Reaches menu | [#106](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/106) |
| FINAL FANTASY XV | CUSA01570 | Does not boot | [#446](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/446) |
| FINAL FANTASY XV EPISODE DUSCAE | CUSA01709 | Reaches menu | [#2260](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2260) |
| FINAL FANTASY XV EPISODE DUSCAE | CUSA01648 | Reaches menu | [#1881](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1881) |
| FINAL FANTASY XV MULTIPLAYER: COMRADES | CUSA13613 | Boots / logos only | [#1906](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1906) |
| FINAL FANTASY XV POCKET EDITION HD | CUSA12648 | Playable | [#1840](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1840) |
| Fist of the North Star Lost Paradise | CUSA12781 | Playable | [#2805](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2805) |
| Five Nights at Freddy's 2 | CUSA18364 | Playable | [#1457](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1457) |
| Five Nights at Freddy's: Security Breach | CUSA24172 | Boots / logos only | [#2823](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2823) |
| FlatOut 4: Total Insanity | CUSA07843 | Ingame (playable-ish) | [#567](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/567) |
| forma.8 | CUSA03406 | Does not boot | [#1433](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1433) |
| Fortnite | CUSA07022 | Boots / logos only | [#1391](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1391) |
| Freevee | CUSA25926 | Boots / logos only | [#1553](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1553) |
| Friday the 13th: The Game | CUSA10007 | Ingame (playable-ish) | [#2120](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2120) |
| Frozen Free Fall: Snowball Fight | CUSA02396 | Playable | [#2285](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2285) |
| Furi | CUSA07961 | Playable | [#1362](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1362) |
| G.I. JOE: Operation Blackout | CUSA20099 | Does not boot | [#2088](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2088) |
| Galak-Z | CUSA02049 | Playable | [#183](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/183) |
| Gear.Club Unlimited 2 Ultimate Edition | CUSA27484 | Does not boot | [#2788](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2788) |
| Genshin Impact | CUSA23681 | Boots / logos only | [#1288](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1288) |
| Ghost Blade HD | CUSA08510 | Playable | [#625](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/625) |
| Ghost Giant | CUSA12664 | Boots / logos only | [#2014](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2014) |
| Ghost of Tsushima | CUSA13323 | Does not boot | [#2089](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2089) |
| Ghost of Tsushima | CUSA11456 | Does not boot | [#1068](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1068) |
| Ghosts 'n Goblins Resurrection | CUSA24159 | Ingame (playable-ish) | [#1844](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1844) |
| Gigantosaurus | CUSA16697 | Ingame (playable-ish) | [#2531](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2531) |
| GIRLS und PANZER Dream Tank Match | CUSA07469 | Playable | [#2819](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2819) |
| Goat Simulator | CUSA02779 | Does not boot | [#2391](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2391) |
| GOD EATER 2 RAGE BURST | CUSA03370 | Reaches menu | [#1713](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1713) |
| GOD EATER RESURRECTION | CUSA03369 | Reaches menu | [#1712](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1712) |
| GOD EATER® 3 | CUSA13137 | Boots / logos only | [#1311](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1311) |
| GOD EATER™ 3 | CUSA13326 | Boots / logos only | [#1792](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1792) |
| God of War | CUSA07408 | Boots / logos only | [#258](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/258) |
| God of War | CUSA07412 | Does not boot | [#2578](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2578) |
| God of War | CUSA07410 | Does not boot | [#1652](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1652) |
| God of War Ragnarök | CUSA34384 | Does not boot | [#1555](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1555) |
| God of War® III Remastered | CUSA01715 | Ingame (playable-ish) | [#711](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/711) |
| God of War® III Remastered | CUSA01623 | Ingame (playable-ish) | [#423](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/423) |
| God of War® III Remastered | CUSA01741 | Boots / logos only | [#1313](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1313) |
| GODZILLA | CUSA02058 | Playable | [#2189](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2189) |
| Golem™ | CUSA04617 | Does not boot | [#2015](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2015) |
| Goosebumps | CUSA18430 | Ingame (playable-ish) | [#2789](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2789) |
| GoPro | CUSA04180 | Boots / logos only | [#1556](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1556) |
| Gran Turismo® 7 | CUSA24769 | Boots / logos only | [#2290](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2290) |
| Gran Turismo®SPORT | CUSA03220 | Boots / logos only | [#2031](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2031) |
| Gran Turismo®SPORT | CUSA03667 | Boots / logos only | [#1314](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1314) |
| Gran Turismo™Sport | CUSA02168 | Does not boot | [#1044](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1044) |
| Granblue Fantasy: Relink | CUSA34766 | Reaches menu | [#1305](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1305) |
| Grand Theft Auto III – The Definitive Edition | CUSA26613 | Reaches menu | [#520](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/520) |
| Grand Theft Auto V | CUSA00419 | Boots / logos only | [#107](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/107) |
| Grand Theft Auto V | CUSA00411 | Does not boot | [#459](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/459) |
| Grand Theft Auto: San Andreas – The Definitive Edition | CUSA26619 | Reaches menu | [#521](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/521) |
| Grand Theft Auto: Vice City – The Definitive Edition | CUSA26616 | Reaches menu | [#522](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/522) |
| Grand Theft Auto: Vice City – The Definitive Edition | CUSA26615 | Boots / logos only | [#2776](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2776) |
| GRAVITY DAZE® 2 | CUSA00547 | Ingame (playable-ish) | [#154](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/154) |
| Gravity Duck | CUSA15330 | Playable | [#2397](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2397) |
| GRAVITY RUSH™ 2 | CUSA04943 | Playable | [#189](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/189) |
| GRAVITY RUSH™ 2 | CUSA03694 | Ingame (playable-ish) | [#1065](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1065) |
| Gravity Rush™ Remastered | CUSA01113 | Ingame (playable-ish) | [#2543](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2543) |
| Gravity Rush™ Remastered | CUSA01130 | Ingame (playable-ish) | [#977](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/977) |
| GRAVITY RUSH™2 DEMO | CUSA06760 | Ingame (playable-ish) | [#2561](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2561) |
| GRID | CUSA14684 | Does not boot | [#2790](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2790) |
| GrimGrimoire OnceMore | CUSA35180 | Ingame (playable-ish) | [#851](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/851) |
| Guacamelee! Super Turbo Championship Edition | CUSA00151 | Playable | [#1882](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1882) |
| Guilty Gear -Strive- | CUSA19210 | Reaches menu | [#2628](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2628) |
| Guilty Gear Xrd -Revelator- | CUSA04112 | Playable | [#1091](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1091) |
| Guilty Gear Xrd -SIGN- | CUSA00834 | Playable | [#868](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/868) |
| Guilty Gear Xrd REV 2 | CUSA07594 | Playable | [#1092](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1092) |
| GUNDAM BREAKER 3 BREAK EDITION | CUSA07651 | Boots / logos only | [#2449](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2449) |
| GUNDAM VERSUS | CUSA08379 | Does not boot | [#1315](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1315) |
| GUNDEMONIUMS | CUSA13032 | Playable | [#624](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/624) |
| Guns Gore and Cannoli 2 | CUSA08317 | Ingame (playable-ish) | [#1883](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1883) |
| GUNS UP!™ | CUSA01483 | Reaches menu | [#1407](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1407) |
| Guns, Gore and Cannoli | CUSA03051 | Playable | [#1884](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1884) |
| H1Z1: Battle Royale | CUSA11745 | Does not boot | [#1408](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1408) |
| Hasbro Family Fun Pack | CUSA03226 | Reaches menu | [#965](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/965) |
| HATSUNE MIKU VR | CUSA17107 | Does not boot | [#2203](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2203) |
| Hatsune Miku: Project DIVA Future Tone | CUSA06093 | Playable | [#1557](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1557) |
| Hatsune Miku: Project DIVA X | CUSA04518 | Ingame (playable-ish) | [#44](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/44) |
| Hatsune Miku: VR Future Live | CUSA04771 | Reaches menu | [#1387](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1387) |
| HBO Max | CUSA41487 | Does not boot | [#1585](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1585) |
| Headset Companion | CUSA00372 | Reaches menu | [#1559](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1559) |
| Headset Companion App | CUSA00468 | Reaches menu | [#2400](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2400) |
| HEAVY RAIN | CUSA02355 | Ingame (playable-ish) | [#842](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/842) |
| HEAVY RAIN™ | CUSA02533 | Ingame (playable-ish) | [#1885](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1885) |
| Hellblade: Senua's Sacrifice™ | CUSA07527 | Reaches menu | [#295](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/295) |
| Hellblade: Senua's Sacrifice™ | CUSA07511 | Boots / logos only | [#1886](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1886) |
| Hello Neighbor | CUSA10963 | Reaches menu | [#2684](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2684) |
| Hentai vs. Evil | CUSA26472 | Reaches menu | [#2464](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2464) |
| Hidden Agenda | CUSA08019 | Reaches menu | [#1887](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1887) |
| Hidden Agenda | CUSA06778 | Does not boot | [#2043](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2043) |
| HITMAN™ | CUSA02369 | Does not boot | [#1409](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1409) |
| HITMAN™ - Definitive Edition | CUSA11947 | Ingame (playable-ish) | [#393](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/393) |
| HITMAN™ - Definitive Edition | CUSA11948 | Does not boot | [#2681](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2681) |
| HITMAN™ 2 | CUSA12401 | Does not boot | [#1064](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1064) |
| HITMAN™ 2 | CUSA12413 | Does not boot | [#395](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/395) |
| Hogwarts Legacy | CUSA12771 | Boots / logos only | [#1449](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1449) |
| Hollow Knight | CUSA13285 | Playable | [#2589](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2589) |
| Homefront®: The Revolution | CUSA00938 | Ingame (playable-ish) | [#404](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/404) |
| Horizon Chase 2 | CUSA39745 | Reaches menu | [#2363](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2363) |
| Horizon Chase Turbo | CUSA11452 | Ingame (playable-ish) | [#2362](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2362) |
| Horizon Forbidden West | CUSA24705 | Boots / logos only | [#2730](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2730) |
| Horizon Zero Dawn: Complete Edition | CUSA10237 | Reaches menu | [#1888](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1888) |
| Horizon Zero Dawn™ | CUSA01021 | Reaches menu | [#1210](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1210) |
| Horizon Zero Dawn™ | CUSA07319 | Boots / logos only | [#2214](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2214) |
| Horizon Zero Dawn™ | CUSA01967 | Boots / logos only | [#283](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/283) |
| Horizon Zero Dawn™ | CUSA05661 | Does not boot | [#468](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/468) |
| Horizon Zero Dawn™: Complete Edition | CUSA10213 | Boots / logos only | [#2728](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2728) |
| Horizon Zero Dawn™: Complete Edition | CUSA10211 | Boots / logos only | [#2544](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2544) |
| HORROR TALES: The Wine | CUSA27869 | Reaches menu | [#2097](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2097) |
| Hotshot Racing | CUSA11585 | Does not boot | [#1370](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1370) |
| Hulu | CUSA00131 | Boots / logos only | [#1560](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1560) |
| I am Setsuna. | CUSA04770 | Ingame (playable-ish) | [#1889](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1889) |
| I Expect You To Die | CUSA05590 | Does not boot | [#2016](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2016) |
| I Expect You To Die 2 | CUSA27600 | Does not boot | [#2017](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2017) |
| ICEY | CUSA08085 | Ingame (playable-ish) | [#1446](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1446) |
| Idle Champions | CUSA11326 | Boots / logos only | [#1561](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1561) |
| Immortals Fenyx Rising ™ | CUSA16345 | Does not boot | [#362](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/362) |
| In the Cloud VR Afterlife | CUSA10566 | Does not boot | [#1563](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1563) |
| InceptionVR | CUSA10117 | Reaches menu | [#1250](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1250) |
| Indivisible | CUSA07000 | Boots / logos only | [#1360](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1360) |
| Indivisible Prototype | CUSA04054 | Playable | [#2576](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2576) |
| inFAMOUS First Light™ | CUSA01064 | Boots / logos only | [#1316](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1316) |
| inFAMOUS Second Son™ | CUSA00223 | Ingame (playable-ish) | [#966](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/966) |
| inFAMOUS Second Son™ | CUSA00309 | Boots / logos only | [#1317](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1317) |
| inFAMOUS™ First Light | CUSA00897 | Ingame (playable-ish) | [#397](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/397) |
| inFAMOUS™ Second Son | CUSA00004 | Ingame (playable-ish) | [#440](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/440) |
| inFAMOUS™ Second Son | CUSA00263 | Reaches menu | [#875](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/875) |
| Injustice: Gods Among Us Ultimate Edition | CUSA00051 | Does not boot | [#2575](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2575) |
| Injustice: Gods Among Us Ultimate Edition | CUSA00079 | Does not boot | [#1349](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1349) |
| InkSplosion | CUSA11651 | Playable | [#2376](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2376) |
| Inscryption | CUSA32884 | Ingame (playable-ish) | [#1385](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1385) |
| INSIDE | CUSA05297 | Playable | [#1891](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1891) |
| Invasion! | CUSA06149 | Does not boot | [#1564](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1564) |
| Iris.Fall | CUSA23259 | Playable | [#2195](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2195) |
| It Takes Two | CUSA16742 | Reaches menu | [#1892](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1892) |
| IxSHE Tell | CUSA17112 | Playable | [#1056](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1056) |
| J-STARS Victory VS+ | CUSA01593 | Ingame (playable-ish) | [#407](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/407) |
| Jackal Assault | CUSA06832 | Ingame (playable-ish) | [#1565](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1565) |
| Jagged Alliance Rage | CUSA08387 | Ingame (playable-ish) | [#408](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/408) |
| Jak 3 | CUSA07841 | Does not boot | [#2791](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2791) |
| Jak X: Combat Racing | CUSA07842 | Does not boot | [#2719](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2719) |
| Jamestown+ | CUSA01008 | Playable | [#1348](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1348) |
| Job Simulator | CUSA05818 | Does not boot | [#2018](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2018) |
| Joe's Diner | CUSA03807 | Ingame (playable-ish) | [#1191](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1191) |
| JoJo's Bizarre Adventure: All-Star Battle R | CUSA28770 | Playable | [#2359](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2359) |
| JoJo's Bizarre Adventure: All-Star Battle R | CUSA28972 | Playable | [#2028](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2028) |
| Joshua Bell VR | CUSA07327 | Boots / logos only | [#1566](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1566) |
| Journey | CUSA00470 | Playable | [#2574](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2574) |
| Journey™ Collector’s Edition | CUSA02172 | Reaches menu | [#1430](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1430) |
| Judgment | CUSA13197 | Ingame (playable-ish) | [#1679](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1679) |
| JUMP FORCE | CUSA11638 | Ingame (playable-ish) | [#1549](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1549) |
| Just Cause 3 | CUSA02747 | Reaches menu | [#2353](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2353) |
| Just Cause 3 | CUSA01493 | Reaches menu | [#771](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/771) |
| Just Cause 4 | CUSA09254 | Reaches menu | [#1567](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1567) |
| Just Cause 4 | CUSA09264 | Reaches menu | [#853](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/853) |
| Just Dance® 2019 | CUSA12549 | Ingame (playable-ish) | [#23](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/23) |
| Kao the Kangaroo | CUSA30891 | Ingame (playable-ish) | [#2161](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2161) |
| Katamari Damacy Reroll | CUSA24361 | Playable | [#812](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/812) |
| Kaze and the Wild Masks | CUSA17979 | Boots / logos only | [#1372](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1372) |
| Keep Talking and Nobody Explodes | CUSA06477 | Does not boot | [#2019](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2019) |
| Kerbal Space Program Enhanced Edition | CUSA11258 | Playable | [#1230](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1230) |
| KeyWe | CUSA25259 | Playable | [#2702](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2702) |
| KILLZONE™ SHADOW FALL | CUSA00008 | Reaches menu | [#2219](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2219) |
| KILLZONE™ SHADOW FALL | CUSA00065 | Reaches menu | [#1318](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1318) |
| KILLZONE™ SHADOW FALL | CUSA00190 | Reaches menu | [#1099](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1099) |
| KILLZONE™ SHADOW FALL | CUSA00002 | Reaches menu | [#533](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/533) |
| Kingdom Come: Deliverance Royal Edition | CUSA15436 | Ingame (playable-ish) | [#923](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/923) |
| KINGDOM HEARTS - HD 1.5+2.5 ReMIX - | CUSA05933 | Ingame (playable-ish) | [#2357](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2357) |
| KINGDOM HEARTS - HD 1.5+2.5 ReMIX - | CUSA05786 | Ingame (playable-ish) | [#523](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/523) |
| KINGDOM HEARTS HD 2.8 FINAL CHAPTER PROLOGUE | CUSA05795 | Ingame (playable-ish) | [#47](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/47) |
| KINGDOM HEARTS Ⅲ | CUSA12025 | Ingame (playable-ish) | [#1660](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1660) |
| KINGDOM HEARTS Ⅲ | CUSA12031 | Ingame (playable-ish) | [#109](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/109) |
| KINGDOM HEARTS Ⅲ | CUSA15072 | Boots / logos only | [#1319](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1319) |
| KINGDOM HEARTS Melody of Memory | CUSA18900 | Ingame (playable-ish) | [#48](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/48) |
| KINGDOM HEARTS Melody of Memory DEMO Version | CUSA23948 | Playable | [#1276](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1276) |
| KINGDOM HEARTS:VR Experience | CUSA15095 | Does not boot | [#1582](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1582) |
| KNACK 2 | CUSA08014 | Does not boot | [#641](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/641) |
| KNACK™ | CUSA00068 | Ingame (playable-ish) | [#713](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/713) |
| KNACK™ | CUSA00006 | Boots / logos only | [#409](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/409) |
| KNACK™ | CUSA00202 | Does not boot | [#344](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/344) |
| KNACK™ 2 | CUSA07670 | Does not boot | [#410](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/410) |
| KNACK™ 2 Demo | CUSA09687 | Does not boot | [#2423](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2423) |
| Knights and Bikes | CUSA13266 | Playable | [#1894](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1894) |
| Korix | CUSA05966 | Does not boot | [#2184](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2184) |
| Kung Fu Panda: Showdown of Legendary Legends | CUSA02591 | Reaches menu | [#2467](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2467) |
| L.A. Noire | CUSA09172 | Ingame (playable-ish) | [#2453](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2453) |
| L.A. Noire | CUSA09084 | Ingame (playable-ish) | [#1902](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1902) |
| Lara Croft and the Temple of Osiris | CUSA00806 | Playable | [#2340](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2340) |
| LEFT ALIVE | CUSA11229 | Reaches menu | [#2533](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2533) |
| LEFT ALIVE | CUSA11201 | Does not boot | [#360](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/360) |
| Legend of Kay Anniversary | CUSA01178 | Ingame (playable-ish) | [#908](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/908) |
| Legends of Catalonia | CUSA13869 | Does not boot | [#1583](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1583) |
| LEGO® CITY UNDERCOVER | CUSA06549 | Playable | [#2114](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2114) |
| LEGO® DIMENSIONS™ | CUSA01176 | Ingame (playable-ish) | [#411](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/411) |
| LEGO® Jurassic World™ | CUSA01519 | Playable | [#389](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/389) |
| LEGO® MARVEL Super Heroes | CUSA00044 | Boots / logos only | [#49](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/49) |
| LEGO® MARVEL Super Heroes 2 | CUSA08476 | Ingame (playable-ish) | [#126](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/126) |
| LEGO® MARVEL's Avengers | CUSA02122 | Playable | [#127](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/127) |
| LEGO® STAR WARS™: The Force Awakens Demo | CUSA03396 | Playable | [#2631](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2631) |
| LEGO® The Hobbit™ | CUSA00355 | Does not boot | [#365](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/365) |
| LEGO® The Incredibles | CUSA09878 | Playable | [#2649](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2649) |
| LEGO® Worlds | CUSA02979 | Playable | [#399](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/399) |
| Lens VR | CUSA06731 | Does not boot | [#1584](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1584) |
| LET IT DIE | CUSA06040 | Does not boot | [#2573](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2573) |
| Lies of P | CUSA36848 | Boots / logos only | [#87](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/87) |
| Life is Strange 2 | CUSA08124 | Ingame (playable-ish) | [#2412](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2412) |
| Life Is Strange™ | CUSA01435 | Playable | [#2591](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2591) |
| Lifeless Planet | CUSA05475 | Ingame (playable-ish) | [#1442](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1442) |
| Like a Dragon Gaiden: The Man Who Erased His Name | CUSA43228 | Does not boot | [#2146](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2146) |
| Like a Dragon: Infinite Wealth | CUSA32137 | Does not boot | [#2181](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2181) |
| Like a Dragon: Pirate Yakuza in Hawaii | CUSA49939 | Does not boot | [#2514](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2514) |
| LIMBO | CUSA01664 | Playable | [#1907](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1907) |
| LittleBigPlanet™3 | CUSA00693 | Boots / logos only | [#2202](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2202) |
| LittleBigPlanet™3 (EU) | CUSA00063 | Boots / logos only | [#366](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/366) |
| LittleBigPlanet™3 (GB) | CUSA00762 | Boots / logos only | [#86](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/86) |
| LittleBigPlanet™3 (US) | CUSA00473 | Boots / logos only | [#642](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/642) |
| LocoRoco™ | CUSA07182 | Does not boot | [#2378](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2378) |
| LocoRoco™ Remastered | CUSA06090 | Playable | [#1397](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1397) |
| LocoRoco™ Remastered | CUSA07286 | Boots / logos only | [#516](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/516) |
| Lords of the Fallen | CUSA00369 | Reaches menu | [#809](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/809) |
| Lost Judgment | CUSA28186 | Does not boot | [#1845](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1845) |
| Mad Max | CUSA00054 | Ingame (playable-ish) | [#888](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/888) |
| Madden NFL 15 | CUSA00526 | Ingame (playable-ish) | [#2327](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2327) |
| Madden NFL 16 | CUSA01691 | Ingame (playable-ish) | [#807](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/807) |
| Madden NFL 16 | CUSA01645 | Does not boot | [#1211](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1211) |
| Madden NFL 19 | CUSA10038 | Reaches menu | [#1908](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1908) |
| Madden NFL 20 | CUSA13265 | Boots / logos only | [#805](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/805) |
| Madden NFL 25 | CUSA00112 | Reaches menu | [#1909](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1909) |
| Mages of Mystralia | CUSA08083 | Ingame (playable-ish) | [#1444](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1444) |
| Magic Knight Grand Charion | CUSA10829 | Ingame (playable-ish) | [#590](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/590) |
| MailApp | CUSA07574 | Reaches menu | [#2735](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2735) |
| Marsupilami Hoobadventure | CUSA27817 | Playable | [#1716](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1716) |
| Marvel's Avengers | CUSA14030 | Does not boot | [#2564](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2564) |
| Marvel's Guardians of the Galaxy | CUSA16704 | Does not boot | [#2253](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2253) |
| Marvel's Spider-Man | CUSA11995 | Ingame (playable-ish) | [#2430](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2430) |
| Marvel's Spider-Man | CUSA02299 | Ingame (playable-ish) | [#60](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/60) |
| Marvel's Spider-Man | CUSA11993 | Boots / logos only | [#581](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/581) |
| Marvel's Spider-Man: Miles Morales | CUSA17776 | Does not boot | [#2592](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2592) |
| Marvel's Spider-Man: Miles Morales | CUSA17722 | Does not boot | [#2254](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2254) |
| Mass Effect™ Legendary Edition | CUSA19515 | Ingame (playable-ish) | [#2780](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2780) |
| Mass Effect™ Legendary Edition | CUSA19500 | Does not boot | [#2512](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2512) |
| Mato Anomalies | CUSA30129 | Does not boot | [#1570](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1570) |
| Matterfall™ | CUSA02475 | Ingame (playable-ish) | [#534](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/534) |
| Mecho Tales | CUSA09693 | Playable | [#1366](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1366) |
| Media Player | CUSA02012 | Reaches menu | [#1586](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1586) |
| MediEvil | CUSA11227 | Ingame (playable-ish) | [#1831](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1831) |
| MediEvil | CUSA12982 | Ingame (playable-ish) | [#412](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/412) |
| Mega Man 11 | CUSA10784 | Playable | [#1910](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1910) |
| Mega Man Legacy Collection 2 | CUSA08268 | Playable | [#1911](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1911) |
| Metal Gear and Metal Gear 2: Solid Snake | CUSA41307 | Reaches menu | [#1153](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1153) |
| METAL GEAR SOLID V: GROUND ZEROES | CUSA00218 | Boots / logos only | [#1912](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1912) |
| METAL GEAR SOLID V: THE DEFINITIVE EXPERIENCE | CUSA05662 | Does not boot | [#1321](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1321) |
| METAL GEAR SOLID V: THE PHANTOM PAIN | CUSA01140 | Boots / logos only | [#457](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/457) |
| METAL SLUG 3 | CUSA02048 | Playable | [#1352](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1352) |
| METAL SLUG XX | CUSA11667 | Playable | [#1914](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1914) |
| Metaphor: ReFantazio | CUSA47038 | Ingame (playable-ish) | [#1184](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1184) |
| Middle-earth™: Shadow of Mordor™ | CUSA00102 | Ingame (playable-ish) | [#1094](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1094) |
| Middle-earth™: Shadow of Mordor™ - Game of the Year Edition | CUSA01939 | Ingame (playable-ish) | [#2501](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2501) |
| Middle-earth™: Shadow of War™ | CUSA04402 | Does not boot | [#2354](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2354) |
| Middle-earth™: Shadow of War™ | CUSA04408 | Does not boot | [#1322](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1322) |
| Midnight Deluxe | CUSA10637 | Reaches menu | [#2398](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2398) |
| Minecraft | CUSA00744 | Ingame (playable-ish) | [#128](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/128) |
| Minecraft | CUSA00265 | Ingame (playable-ish) | [#120](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/120) |
| Minecraft Dungeons | CUSA18797 | Ingame (playable-ish) | [#1675](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1675) |
| Minecraft: Story Mode | CUSA06409 | Playable | [#1664](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1664) |
| Minecraft: Story Mode | CUSA02430 | Ingame (playable-ish) | [#323](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/323) |
| MiniWood VR | CUSA12871 | Boots / logos only | [#917](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/917) |
| Mirror's Edge™ Catalyst | CUSA01566 | Ingame (playable-ish) | [#1915](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1915) |
| Mirror's Edge™ Catalyst | CUSA01499 | Ingame (playable-ish) | [#390](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/390) |
| MLB® 15 The Show™ | CUSA00998 | Reaches menu | [#804](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/804) |
| MLB® The Show™ 19 | CUSA13355 | Reaches menu | [#2607](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2607) |
| MLB® The Show™ 24 | CUSA43942 | Reaches menu | [#2135](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2135) |
| Mobile Suit Gundam Battle Operation Code Fairy | CUSA28763 | Does not boot | [#1650](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1650) |
| MOBILE SUIT GUNDAM EXTREME VS. MAXIBOOST ON | CUSA15000 | Does not boot | [#241](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/241) |
| MOBILE SUIT GUNDAM EXTREME VS. 極限爆發 | CUSA15005 | Does not boot | [#1790](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1790) |
| MONOPOLY FAMILY FUN PACK | CUSA01317 | Playable | [#802](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/802) |
| MonopolyMadness | CUSA27124 | Playable | [#2461](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2461) |
| Monster Boy and the Cursed Kingdom | CUSA05011 | Playable | [#1437](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1437) |
| Monster Hunter World: Iceborne | CUSA07713 | Reaches menu | [#129](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/129) |
| Monster Hunter World: Iceborne | CUSA07708 | Boots / logos only | [#2594](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2594) |
| Mortal Kombat 11 | CUSA11395 | Does not boot | [#2175](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2175) |
| Mortal Kombat X | CUSA00970 | Ingame (playable-ish) | [#2571](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2571) |
| Mortal Kombat XL | CUSA03679 | Ingame (playable-ish) | [#2572](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2572) |
| Mortal Shell | CUSA20133 | Ingame (playable-ish) | [#2420](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2420) |
| Moss | CUSA09290 | Does not boot | [#1129](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1129) |
| Moss: Book II | CUSA23252 | Does not boot | [#2020](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2020) |
| Mount & Blade: Warband | CUSA03263 | Playable | [#1202](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1202) |
| MouseCraft | CUSA00654 | Boots / logos only | [#1703](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1703) |
| Moving Out | CUSA17675 | Playable | [#2454](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2454) |
| Murdered: Soul Suspect | CUSA00342 | Playable | [#2798](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2798) |
| Mutant Year Zero: Road to Eden | CUSA12667 | Boots / logos only | [#572](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/572) |
| My First Gran Turismo® | CUSA49696 | Boots / logos only | [#1587](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1587) |
| MY HERO ULTRA RUMBLE | CUSA27946 | Boots / logos only | [#1588](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1588) |
| My Name is Mayo | CUSA09386 | Playable | [#2390](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2390) |
| My Name is Mayo 2 | CUSA25283 | Playable | [#2392](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2392) |
| NBA | CUSA06996 | Does not boot | [#1589](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1589) |
| NBA 2K Playgrounds 2 | CUSA13619 | Boots / logos only | [#2236](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2236) |
| NBA 2K14 | CUSA00007 | Does not boot | [#800](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/800) |
| NBA 2K14 | CUSA00104 | Does not boot | [#121](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/121) |
| NBA 2K15 | CUSA00768 | Ingame (playable-ish) | [#2608](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2608) |
| NBA 2K15 | CUSA00787 | Ingame (playable-ish) | [#20](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/20) |
| NBA 2K16 | CUSA02764 | Reaches menu | [#2440](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2440) |
| NBA 2K17 | CUSA05040 | Reaches menu | [#2609](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2609) |
| NBA 2K17 | CUSA05036 | Boots / logos only | [#123](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/123) |
| NBA 2K17: The Prelude | CUSA05035 | Boots / logos only | [#2547](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2547) |
| NBA 2K18 | CUSA08500 | Reaches menu | [#130](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/130) |
| NBA 2K19 | CUSA12476 | Does not boot | [#2610](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2610) |
| NBA 2K19 | CUSA12525 | Does not boot | [#2548](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2548) |
| NBA 2K20 | CUSA16386 | Does not boot | [#2549](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2549) |
| NBA 2K22 | CUSA28235 | Does not boot | [#2347](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2347) |
| NBA LIVE 14 | CUSA00114 | Playable | [#799](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/799) |
| NBA LIVE 19 | CUSA11355 | Does not boot | [#430](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/430) |
| NBC Sports | CUSA11553 | Does not boot | [#1590](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1590) |
| Need for Speed™ | CUSA01925 | Reaches menu | [#2294](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2294) |
| Need for Speed™ | CUSA01866 | Reaches menu | [#1643](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1643) |
| Need for Speed™ Heat | CUSA15090 | Reaches menu | [#1711](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1711) |
| Need for Speed™ Heat | CUSA15081 | Reaches menu | [#925](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/925) |
| Need for Speed™ Hot Pursuit Remastered | CUSA23264 | Playable | [#2822](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2822) |
| Need for Speed™ Hot Pursuit Remastered | CUSA23265 | Playable | [#1696](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1696) |
| Need for Speed™ Payback | CUSA05986 | Reaches menu | [#1710](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1710) |
| Need for Speed™ Rivals | CUSA00113 | Playable | [#2200](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2200) |
| Need for Speed™ Rivals | CUSA00168 | Ingame (playable-ish) | [#1640](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1640) |
| NEEDY GIRL OVERDOSE | CUSA50171 | Playable | [#2360](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2360) |
| NEO : The World Ends with You | CUSA26377 | Ingame (playable-ish) | [#2171](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2171) |
| NEO : The World Ends with You | CUSA26376 | Ingame (playable-ish) | [#55](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/55) |
| Neptunia x SENRAN KAGURA: Ninja Wars | CUSA29604 | Ingame (playable-ish) | [#1834](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1834) |
| Netflix | CUSA00127 | Reaches menu | [#2399](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2399) |
| Netflix | CUSA00129 | Boots / logos only | [#1591](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1591) |
| Never Alone | CUSA01305 | Ingame (playable-ish) | [#1916](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1916) |
| New GUNDAM BREAKER | CUSA11744 | Boots / logos only | [#1323](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1323) |
| New Super Lucky's Tale | CUSA20229 | Playable | [#901](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/901) |
| Nexomon: Extinction | CUSA19639 | Playable | [#1375](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1375) |
| NHL® 19 | CUSA11126 | Reaches menu | [#2683](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2683) |
| NHL® 24 | CUSA37933 | Does not boot | [#2712](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2712) |
| NHL™ 15 | CUSA00561 | Does not boot | [#1212](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1212) |
| NHL™ 19 | CUSA11116 | Ingame (playable-ish) | [#2632](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2632) |
| Ni no Kuni Wrath of the White Witch™ Remastered | CUSA13079 | Ingame (playable-ish) | [#2452](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2452) |
| Ni no Kuni™ II: Revenant Kingdom | CUSA07345 | Playable | [#663](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/663) |
| Ni no Kuni™ II: Revenant Kingdom | CUSA09243 | Ingame (playable-ish) | [#524](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/524) |
| NieR Replicant ver.1.22474487139... | CUSA18471 | Ingame (playable-ish) | [#2134](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2134) |
| NieR Replicant ver.1.22474487139... | CUSA18774 | Ingame (playable-ish) | [#1170](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1170) |
| NieR Replicant ver.1.22474487139... | CUSA18742 | Ingame (playable-ish) | [#124](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/124) |
| NieR:Automata | CUSA04551 | Ingame (playable-ish) | [#835](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/835) |
| NieR:Automata | CUSA04480 | Ingame (playable-ish) | [#81](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/81) |
| NieR:Automata DEMO 120161128 | CUSA07007 | Ingame (playable-ish) | [#1410](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1410) |
| Night in the Woods | CUSA05447 | Playable | [#1917](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1917) |
| Night Trap - 25th Anniversary Edition | CUSA07957 | Playable | [#1443](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1443) |
| Nioh | CUSA07113 | Reaches menu | [#1089](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1089) |
| Nioh | CUSA07123 | Boots / logos only | [#451](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/451) |
| Nioh 2 | CUSA15532 | Boots / logos only | [#1918](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1918) |
| Nippon Marathon | CUSA11337 | Boots / logos only | [#2727](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2727) |
| No Man's Sky | CUSA03952 | Reaches menu | [#2595](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2595) |
| No Man's Sky | CUSA04841 | Boots / logos only | [#2690](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2690) |
| Oceanhorn | CUSA05010 | Playable | [#1920](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1920) |
| Octodad: Dadliest Catch | CUSA00655 | Boots / logos only | [#2339](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2339) |
| OCTOPATH TRAVELER II | CUSA36274 | Ingame (playable-ish) | [#1657](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1657) |
| Odin Sphere Leifthrasir | CUSA05083 | Playable | [#1439](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1439) |
| Odin Sphere Leifthrasir | CUSA01290 | Playable | [#866](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/866) |
| Ōkami HD | CUSA08364 | Ingame (playable-ish) | [#855](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/855) |
| ŌKAMI HD | CUSA08418 | Ingame (playable-ish) | [#861](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/861) |
| OLYMPIC GAMES TOKYO 2020™ | CUSA11208 | Boots / logos only | [#2510](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2510) |
| Omega Quintet | CUSA01951 | Does not boot | [#1680](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1680) |
| ONRUSH™ | CUSA10887 | Boots / logos only | [#2713](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2713) |
| ONRUSH™ | CUSA09559 | Boots / logos only | [#2657](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2657) |
| Outer Wilds | CUSA09919 | Reaches menu | [#1384](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1384) |
| OUTRIDERS | CUSA08043 | Does not boot | [#1135](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1135) |
| Overcooked | CUSA05724 | Playable | [#1921](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1921) |
| Overcooked 2 | CUSA10940 | Playable | [#1922](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1922) |
| Overcooked! All You Can Eat | CUSA23464 | Playable | [#1923](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1923) |
| Overwatch: Origins Edition | CUSA01842 | Reaches menu | [#1175](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1175) |
| P.T. | CUSA01114 | Ingame (playable-ish) | [#2226](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2226) |
| P.T. | CUSA01127 | Ingame (playable-ish) | [#1924](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1924) |
| PAC-MAN™ CHAMPIONSHIP EDITION 2 | CUSA04944 | Playable | [#2570](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2570) |
| PAC-MAN™ Championship Edition 2 + Arcade Game Series™ | CUSA06906 | Ingame (playable-ish) | [#2206](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2206) |
| Paladins | CUSA05078 | Reaches menu | [#2424](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2424) |
| Paragon | CUSA04186 | Boots / logos only | [#2425](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2425) |
| Paramount+ | CUSA05365 | Does not boot | [#1592](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1592) |
| Paranormal Activity: The Lost Soul | CUSA08742 | Boots / logos only | [#2368](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2368) |
| PaRappa The Rapper™ Remastered | CUSA05289 | Playable | [#2463](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2463) |
| Path of Exile | CUSA11782 | Reaches menu | [#2569](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2569) |
| PAYDAY 2: CRIMEWAVE EDITION | CUSA01770 | Ingame (playable-ish) | [#2216](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2216) |
| PAYDAY 2: CRIMEWAVE EDITION | CUSA01761 | Ingame (playable-ish) | [#1411](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1411) |
| PDP Cloud Remote App | CUSA10894 | Reaches menu | [#1593](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1593) |
| PeacockTV | CUSA20387 | Does not boot | [#1594](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1594) |
| Peggle 2 | CUSA00743 | Playable | [#2523](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2523) |
| Persona 3 Portable | CUSA33871 | Playable | [#2642](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2642) |
| Persona 3 Reload | CUSA37522 | Ingame (playable-ish) | [#2286](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2286) |
| Persona 3 Reload | CUSA37521 | Ingame (playable-ish) | [#132](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/132) |
| Persona 3: Dancing in Moonlight | CUSA12636 | Ingame (playable-ish) | [#1841](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1841) |
| Persona 4 Arena Ultimax | CUSA27209 | Playable | [#1134](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1134) |
| Persona 4 GOLDEN | CUSA33874 | Ingame (playable-ish) | [#1106](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1106) |
| Persona 4: Dancing All Night | CUSA12811 | Playable | [#1111](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1111) |
| Persona 5 | CUSA05877 | Ingame (playable-ish) | [#2288](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2288) |
| Persona 5 | CUSA06638 | Ingame (playable-ish) | [#2287](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2287) |
| Persona 5 Royal | CUSA17419 | Ingame (playable-ish) | [#133](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/133) |
| Persona 5 Strikers | CUSA19640 | Reaches menu | [#134](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/134) |
| Persona 5: Dancing in Starlight | CUSA12380 | Ingame (playable-ish) | [#1808](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1808) |
| PGA TOUR 2K21 | CUSA19467 | Boots / logos only | [#1337](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1337) |
| Phantasy Star Online 2 New Genesis | CUSA29813 | Reaches menu | [#1595](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1595) |
| Pinball Arcade | CUSA00212 | Ingame (playable-ish) | [#850](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/850) |
| Pinball Arcade | CUSA01828 | Ingame (playable-ish) | [#849](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/849) |
| Pinball FX | CUSA34954 | Boots / logos only | [#1596](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1596) |
| Pix the Cat | CUSA01352 | Ingame (playable-ish) | [#1427](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1427) |
| PixelJunk™ Monsters 2 | CUSA11967 | Ingame (playable-ish) | [#1929](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1929) |
| PLATINUM DEMO – FINAL FANTASY XV | CUSA04514 | Does not boot | [#2261](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2261) |
| PLATINUM DEMO – FINAL FANTASY XV | CUSA04568 | Does not boot | [#1930](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1930) |
| PlayStation® VR WORLDS | CUSA05202 | Does not boot | [#2792](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2792) |
| PlayStation® VR WORLDS | CUSA01690 | Does not boot | [#1511](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1511) |
| PlayStation®VR Demo | CUSA06853 | Does not boot | [#1394](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1394) |
| PlayStation®VR Demo Disc | CUSA04579 | Does not boot | [#2022](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2022) |
| PlayStation®VR Demo Disc 2 | CUSA09142 | Boots / logos only | [#1597](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1597) |
| PlayStation®VR Demo Disc 2 | CUSA09159 | Boots / logos only | [#1543](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1543) |
| PlayStation®VR Demo Disc 3 | CUSA13845 | Boots / logos only | [#1544](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1544) |
| Plex | CUSA01850 | Does not boot | [#1598](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1598) |
| Pluto TV | CUSA04688 | Does not boot | [#1599](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1599) |
| Potion Permit | CUSA33783 | Playable | [#929](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/929) |
| PowerWash Simulator | CUSA29983 | Reaches menu | [#59](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/59) |
| Prey | CUSA04482 | Ingame (playable-ish) | [#2345](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2345) |
| Project CARS | CUSA00940 | Playable | [#1107](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1107) |
| Project CARS GOTY Edition | CUSA04932 | Reaches menu | [#1336](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1336) |
| Project Highrise: Architect's Edition | CUSA11463 | Ingame (playable-ish) | [#2052](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2052) |
| Project Oak Tree | CUSA20245 | Ingame (playable-ish) | [#2614](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2614) |
| Psychonauts™ | CUSA03881 | Does not boot | [#1436](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1436) |
| Pure Farming 2018 | CUSA09194 | Ingame (playable-ish) | [#594](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/594) |
| Puyo Puyo™ Tetris® | CUSA06509 | Playable | [#2416](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2416) |
| Puyo Puyo™ Tetris® 2 | CUSA24365 | Playable | [#2524](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2524) |
| Quake | CUSA24593 | Ingame (playable-ish) | [#2178](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2178) |
| R-TYPE FINAL 2 | CUSA25458 | Reaches menu | [#596](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/596) |
| Race With Ryan | CUSA15718 | Ingame (playable-ish) | [#2045](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2045) |
| RaceTheSun | CUSA00708 | Boots / logos only | [#1350](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1350) |
| Rad TV | CUSA06120 | Boots / logos only | [#1476](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1476) |
| RAGE2 | CUSA10339 | Reaches menu | [#2685](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2685) |
| RAGE2 | CUSA10300 | Reaches menu | [#618](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/618) |
| Ratchet & Clank™ | CUSA01928 | Ingame (playable-ish) | [#2568](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2568) |
| Ratchet & Clank™ | CUSA01047 | Ingame (playable-ish) | [#359](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/359) |
| Ratchet & Clank™ | CUSA01073 | Ingame (playable-ish) | [#125](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/125) |
| Rayman® Legends | CUSA00069 | Playable | [#765](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/765) |
| Rayman® Legends | CUSA00031 | Playable | [#368](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/368) |
| Realms of Arkania: Blade of Destiny | CUSA02745 | Reaches menu | [#2100](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2100) |
| Rec Room | CUSA08481 | Boots / logos only | [#1601](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1601) |
| Red Bull TV | CUSA03460 | Does not boot | [#1603](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1603) |
| Red Dead Redemption | CUSA36842 | Ingame (playable-ish) | [#1541](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1541) |
| Red Dead Redemption 2 | CUSA08519 | Reaches menu | [#2550](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2550) |
| Red Dead Redemption 2 | CUSA03041 | Reaches menu | [#926](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/926) |
| Redbox | CUSA30257 | Does not boot | [#1602](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1602) |
| Redout | CUSA08639 | Reaches menu | [#2217](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2217) |
| Redout 2 | CUSA31411 | Ingame (playable-ish) | [#2218](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2218) |
| RESIDENT EVIL 2 | CUSA09193 | Reaches menu | [#318](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/318) |
| RESIDENT EVIL 2 | CUSA09171 | Does not boot | [#1900](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1900) |
| RESIDENT EVIL 3 | CUSA14168 | Does not boot | [#1132](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1132) |
| RESIDENT EVIL 3 "Raccoon City Demo" | CUSA17925 | Does not boot | [#1412](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1412) |
| resident evil 4 | CUSA04704 | Ingame (playable-ish) | [#2371](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2371) |
| resident evil 4 | CUSA04885 | Ingame (playable-ish) | [#177](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/177) |
| Resident Evil 4 | CUSA33388 | Does not boot | [#2596](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2596) |
| Resident Evil 4 | CUSA33387 | Does not boot | [#2095](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2095) |
| RESIDENT EVIL 6 | CUSA03856 | Ingame (playable-ish) | [#345](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/345) |
| RESIDENT EVIL 7 biohazard | CUSA03842 | Ingame (playable-ish) | [#2172](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2172) |
| RESIDENT EVIL 7 biohazard | CUSA03962 | Ingame (playable-ish) | [#945](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/945) |
| RESIDENT EVIL 7 biohazard Gold Edition | CUSA09643 | Ingame (playable-ish) | [#1931](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1931) |
| Resident Evil 7 Teaser: Beginning Hour | CUSA04772 | Reaches menu | [#1932](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1932) |
| Resident Evil Origins Collection | CUSA03178 | Playable | [#322](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/322) |
| RESIDENT EVIL RESISTANCE | CUSA14169 | Does not boot | [#1140](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1140) |
| RESIDENT EVIL REVELATIONS | CUSA06314 | Ingame (playable-ish) | [#2298](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2298) |
| RESIDENT EVIL REVELATIONS 2 | CUSA01141 | Playable | [#1933](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1933) |
| Resident Evil Village | CUSA18008 | Does not boot | [#2597](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2597) |
| RESIDENT EVIL5 | CUSA04437 | Ingame (playable-ish) | [#292](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/292) |
| Resident Evil™ | CUSA01067 | Ingame (playable-ish) | [#2598](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2598) |
| RESOGUN™ | CUSA00038 | Playable | [#1934](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1934) |
| Reverie | CUSA11616 | Playable | [#595](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/595) |
| RIGS Mechanized Combat League™ | CUSA02983 | Does not boot | [#2793](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2793) |
| RiME | CUSA05796 | Ingame (playable-ish) | [#1935](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1935) |
| Rise of the Tomb Raider | CUSA05794 | Ingame (playable-ish) | [#766](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/766) |
| Rise of the Tomb Raider | CUSA05716 | Boots / logos only | [#1213](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1213) |
| Rise of the Tomb Raider | CUSA06059 | Does not boot | [#1326](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1326) |
| Road Bustle | CUSA24422 | Does not boot | [#2388](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2388) |
| Robinson: The Journey | CUSA05850 | Does not boot | [#2821](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2821) |
| Roblox | CUSA06122 | Does not boot | [#2138](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2138) |
| ROCK BAND 4 | CUSA02084 | Ingame (playable-ish) | [#1855](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1855) |
| Rocket League® | CUSA01163 | Ingame (playable-ish) | [#794](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/794) |
| Rocket League™ | CUSA01433 | Ingame (playable-ish) | [#526](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/526) |
| Rocksmith 2014 | CUSA00745 | Does not boot | [#1386](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1386) |
| Rogue Legacy | CUSA00629 | Playable | [#1936](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1936) |
| RollerCoaster Tycoon Joyride | CUSA09879 | Boots / logos only | [#2794](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2794) |
| Ronin | CUSA05362 | Playable | [#2116](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2116) |
| RPG Maker MV Player | CUSA14395 | Reaches menu | [#1604](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1604) |
| Rust | CUSA14307 | Reaches menu | [#1171](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1171) |
| Rusty Spout Rescue Adventure | CUSA25257 | Playable | [#1714](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1714) |
| Sackboy: A Big Adventure | CUSA18867 | Boots / logos only | [#2369](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2369) |
| Sackboy™: A Big Adventure | CUSA18886 | Ingame (playable-ish) | [#2587](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2587) |
| Sackboy™: A Big Adventure | CUSA06313 | Boots / logos only | [#2387](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2387) |
| Saints Row IV: Re-Elected | CUSA01106 | Ingame (playable-ish) | [#1413](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1413) |
| Saints Row: Gat out of Hell | CUSA00939 | Ingame (playable-ish) | [#1414](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1414) |
| Sakuna: Of Rice and Ruin | CUSA09130 | Does not boot | [#2149](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2149) |
| Sakura Wars | CUSA16404 | Ingame (playable-ish) | [#2356](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2356) |
| Sakura Wars | CUSA16429 | Ingame (playable-ish) | [#540](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/540) |
| Salt and Sanctuary | CUSA02353 | Boots / logos only | [#1431](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1431) |
| SAMURAI WARRIORS 4 | CUSA00783 | Reaches menu | [#2405](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2405) |
| Scarlet Nexus | CUSA25132 | Boots / logos only | [#939](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/939) |
| SCARLET NEXUS | CUSA25269 | Ingame (playable-ish) | [#2299](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2299) |
| Scott Pilgrim vs. the World: The Game | CUSA20085 | Playable | [#1937](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1937) |
| SEGA Genesis Classics | CUSA10828 | Playable | [#970](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/970) |
| Sekiro™: Shadows Die Twice | CUSA12047 | Ingame (playable-ish) | [#1938](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1938) |
| Sekiro™: Shadows Die Twice | CUSA13801 | Ingame (playable-ish) | [#841](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/841) |
| SENRAN KAGURA Burst Re:Newal | CUSA11712 | Does not boot | [#1692](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1692) |
| Severed Steel | CUSA30138 | Ingame (playable-ish) | [#2521](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2521) |
| Shadow Man Remastered | CUSA25661 | Playable | [#2372](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2372) |
| Shadow of the Beast™ | CUSA03762 | Ingame (playable-ish) | [#845](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/845) |
| SHADOW OF THE COLOSSUS | CUSA08034 | Reaches menu | [#324](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/324) |
| SHADOW OF THE COLOSSUS™ | CUSA08809 | Reaches menu | [#502](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/502) |
| SHADOW OF THE COLOSSUS™ | CUSA08804 | Boots / logos only | [#1327](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1327) |
| Shadow of the Tomb Raider | CUSA10938 | Boots / logos only | [#792](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/792) |
| Shadow Warrior | CUSA00628 | Playable | [#1097](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1097) |
| Shahid | CUSA40504 | Does not boot | [#1606](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1606) |
| Shantae | CUSA34280 | Boots / logos only | [#1939](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1939) |
| Shantae and the Pirate's Curse | CUSA01609 | Playable | [#1940](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1940) |
| Shantae: Half-Genie Hero Ultimate Edition | CUSA11441 | Ingame (playable-ish) | [#1941](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1941) |
| Shantae: Risky's Revenge - Director's Cut | CUSA01587 | Ingame (playable-ish) | [#1942](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1942) |
| SHAREfactory™ | CUSA00572 | Boots / logos only | [#1607](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1607) |
| Shenmue | CUSA12279 | Ingame (playable-ish) | [#1943](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1943) |
| Shenmue II | CUSA12280 | Playable | [#1944](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1944) |
| Shenmue III | CUSA12289 | Ingame (playable-ish) | [#1945](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1945) |
| Shikhondo - 食魂徒 | CUSA11603 | Playable | [#623](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/623) |
| Shin Megami Tensei III Nocturne HD Remaster | CUSA24919 | Playable | [#1189](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1189) |
| Shin Megami Tensei III Nocturne HD Remaster | CUSA24920 | Ingame (playable-ish) | [#2620](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2620) |
| SingStar™ Ultimate Party | CUSA00501 | Does not boot | [#1214](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1214) |
| Skai | CUSA00449 | Does not boot | [#2162](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2162) |
| Skullgirls 2nd Encore | CUSA01606 | Playable | [#1608](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1608) |
| Skulls of the Shogun | CUSA02050 | Ingame (playable-ish) | [#1147](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1147) |
| Sky Force Reloaded | CUSA09428 | Reaches menu | [#1718](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1718) |
| Skylanders™ SuperChargers | CUSA02180 | Playable | [#2337](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2337) |
| Skyrim | CUSA05486 | Ingame (playable-ish) | [#1662](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1662) |
| Skyrim | CUSA05333 | Ingame (playable-ish) | [#1507](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1507) |
| Skyrim VR | CUSA09124 | Does not boot | [#2024](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2024) |
| Slime Rancher | CUSA11650 | Ingame (playable-ish) | [#2341](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2341) |
| Slime Rancher | CUSA11587 | Ingame (playable-ish) | [#1259](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1259) |
| Snatch VR Heist Experience | CUSA08536 | Does not boot | [#1610](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1610) |
| Sniper Elite 3 | CUSA00378 | Boots / logos only | [#816](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/816) |
| Sniper Elite 4 | CUSA04220 | Ingame (playable-ish) | [#724](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/724) |
| Sniper Ghost Warrior 3 | CUSA04868 | Reaches menu | [#783](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/783) |
| Song of the Deep | CUSA05049 | Playable | [#1438](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1438) |
| SONIC FORCES | CUSA05637 | Playable | [#786](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/786) |
| SONIC FRONTIERS | CUSA28194 | Ingame (playable-ish) | [#2289](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2289) |
| Sonic Mania | CUSA07023 | Playable | [#1611](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1611) |
| Sonic Origins | CUSA30448 | Playable | [#2270](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2270) |
| Sony Pictures Core | CUSA44977 | Does not boot | [#1612](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1612) |
| Soul Hackers 2 | CUSA27730 | Ingame (playable-ish) | [#2168](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2168) |
| SOULCALIBUR™Ⅵ | CUSA09903 | Ingame (playable-ish) | [#2820](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2820) |
| SOULCALIBUR™Ⅵ | CUSA09884 | Ingame (playable-ish) | [#2111](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2111) |
| Sound Shapes | CUSA00090 | Playable | [#1649](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1649) |
| South Park™: The Fractured But Whole™ | CUSA05485 | Ingame (playable-ish) | [#352](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/352) |
| South Park™: The Fractured But Whole™ | CUSA04311 | Ingame (playable-ish) | [#343](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/343) |
| South Park™: The Stick of Truth™ | CUSA04696 | Ingame (playable-ish) | [#1719](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1719) |
| Space Junkies | CUSA14019 | Does not boot | [#2185](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2185) |
| Spelunky | CUSA00493 | Playable | [#2525](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2525) |
| Spelunky 2 | CUSA20601 | Boots / logos only | [#2177](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2177) |
| Splitgate: Arena Warfare | CUSA26225 | Reaches menu | [#2734](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2734) |
| SpongeBob SquarePants: Battle For Bikini Bottom - Rehydrated | CUSA14898 | Ingame (playable-ish) | [#1168](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1168) |
| SpongeBob SquarePants: The Cosmic Shake | CUSA30579 | Reaches menu | [#1054](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1054) |
| Spotify | CUSA01780 | Does not boot | [#1613](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1613) |
| Spyro Reignited Trilogy | CUSA12125 | Ingame (playable-ish) | [#326](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/326) |
| STAR OCEAN First Departure R | CUSA16866 | Playable | [#2621](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2621) |
| STAR OCEAN THE DIVINE FORCE | CUSA31855 | Reaches menu | [#2622](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2622) |
| STAR OCEAN THE SECOND STORY R | CUSA40652 | Reaches menu | [#509](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/509) |
| STAR OCEAN: Integrity and Faithlessness | CUSA03219 | Boots / logos only | [#538](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/538) |
| STAR OCEAN: Integrity and Faithlessness | CUSA03246 | Does not boot | [#1355](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1355) |
| Star Ocean®: Till The End Of Time™ | CUSA06379 | Does not boot | [#2551](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2551) |
| Star Renegades | CUSA23211 | Ingame (playable-ish) | [#1378](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1378) |
| Star Wars Bounty Hunter™ | CUSA03472 | Does not boot | [#1356](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1356) |
| Star Wars Jedi Knight II: Jedi Outcast | CUSA16655 | Ingame (playable-ish) | [#2612](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2612) |
| STAR WARS Jedi: Fallen Order™ | CUSA12539 | Boots / logos only | [#1614](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1614) |
| STAR WARS™ Battlefront™ | CUSA00640 | Reaches menu | [#1051](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1051) |
| STAR WARS™ Battlefront™ | CUSA00634 | Reaches menu | [#270](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/270) |
| STAR WARS™ Battlefront™ II Multiplayer Beta | CUSA08752 | Does not boot | [#1415](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1415) |
| Star Wars™: Racer Revenge™ | CUSA03474 | Does not boot | [#2623](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2623) |
| Stardew Valley | CUSA06840 | Does not boot | [#2795](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2795) |
| Stardew Valley | CUSA26625 | Does not boot | [#2220](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2220) |
| Starlink: Battle for Atlas™ | CUSA06938 | Boots / logos only | [#371](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/371) |
| Steel Rats | CUSA12994 | Ingame (playable-ish) | [#1159](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1159) |
| STEEP | CUSA05527 | Boots / logos only | [#1416](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1416) |
| STEEP | CUSA05489 | Boots / logos only | [#996](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/996) |
| STEINS;GATE ELITE | CUSA13222 | Boots / logos only | [#1392](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1392) |
| STRANGER OF PARADISE FINAL FANTASY ORIGIN | CUSA29578 | Reaches menu | [#2098](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2098) |
| Stray | CUSA24899 | Boots / logos only | [#1695](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1695) |
| Stray | CUSA24898 | Boots / logos only | [#902](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/902) |
| STREET FIGHTER V | CUSA01200 | Ingame (playable-ish) | [#957](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/957) |
| Streets of Rage 4 | CUSA16237 | Does not boot | [#1946](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1946) |
| Styx: Master of Shadows | CUSA01069 | Boots / logos only | [#1108](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1108) |
| Subnautica | CUSA13893 | Reaches menu | [#2624](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2624) |
| Sundered: Eldritch Edition | CUSA07282 | Ingame (playable-ish) | [#1361](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1361) |
| Super Comboman: Smash Edition | CUSA05191 | Playable | [#1440](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1440) |
| Super Destronaut DX | CUSA12554 | Playable | [#2411](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2411) |
| Super Meat Boy Forever | CUSA16602 | Playable | [#2526](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2526) |
| Super Meat Boy! | CUSA03454 | Playable | [#2527](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2527) |
| Super Monkey Ball Banana Blitz HD | CUSA16178 | Playable | [#1723](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1723) |
| Super Stardust™Ultra VR | CUSA05951 | Boots / logos only | [#2445](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2445) |
| Super Weekend Mode | CUSA15131 | Playable | [#2389](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2389) |
| Surgeon Simulator | CUSA00861 | Ingame (playable-ish) | [#2760](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2760) |
| SWORD ART ONLINE Alicization Lycoris | CUSA18374 | Does not boot | [#1144](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1144) |
| SWORD ART ONLINE Last Recollection | CUSA32810 | Reaches menu | [#1616](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1616) |
| SWORD ART ONLINE Re: Hollow Fragment | CUSA02560 | Playable | [#1617](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1617) |
| SWORD ART ONLINE: FATAL BULLET | CUSA10093 | Ingame (playable-ish) | [#1615](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1615) |
| SWORD ART ONLINE: HOLLOW REALIZATION | CUSA05125 | Playable | [#138](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/138) |
| Sword Art Online: Lost Song | CUSA02823 | Playable | [#139](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/139) |
| Table Top Racing: World Tour | CUSA01498 | Playable | [#2381](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2381) |
| Taiko no Tatsujin: Drum Session! | CUSA11106 | Does not boot | [#2234](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2234) |
| Tales from the Borderlands | CUSA03817 | Playable | [#789](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/789) |
| Tales of Arise | CUSA17225 | Does not boot | [#2625](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2625) |
| Tales of Berseria | CUSA05258 | Does not boot | [#1441](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1441) |
| Tales of Vesperia: Definitive Edition | CUSA12287 | Ingame (playable-ish) | [#2626](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2626) |
| Tales of Zestiria™ | CUSA02461 | Playable | [#2108](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2108) |
| Tales of Zestiria™ | CUSA02510 | Ingame (playable-ish) | [#1432](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1432) |
| Taxi Chaos | CUSA20527 | Ingame (playable-ish) | [#1187](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1187) |
| Tearaway® Unfolded | CUSA01607 | Reaches menu | [#703](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/703) |
| Tearaway™ Unfolded | CUSA00562 | Boots / logos only | [#398](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/398) |
| Teenage Mutant Ninja Turtles: Shredder's Revenge | CUSA30991 | Reaches menu | [#893](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/893) |
| Teenage Mutant Ninja Turtles: The Cowabunga Collection | CUSA29248 | Playable | [#2555](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2555) |
| Teenage Mutant Ninja Turtles™: Mutants in Manhattan | CUSA01843 | Playable | [#427](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/427) |
| TEKKEN™7 | CUSA05972 | Ingame (playable-ish) | [#1947](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1947) |
| TEKKEN™7 | CUSA06014 | Ingame (playable-ish) | [#483](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/483) |
| Tempest 4000 | CUSA09856 | Playable | [#768](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/768) |
| TENNIS TV | CUSA15736 | Does not boot | [#1618](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1618) |
| Terra Bomber | CUSA23558 | Boots / logos only | [#2759](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2759) |
| Terra Lander | CUSA18893 | Reaches menu | [#2757](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2757) |
| Terra Lander II: Rockslide Rescue | CUSA18894 | Boots / logos only | [#2758](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2758) |
| Terraria | CUSA00740 | Playable | [#2558](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2558) |
| Terraria | CUSA00737 | Does not boot | [#527](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/527) |
| TerraTech | CUSA12877 | Playable | [#1619](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1619) |
| That's You!™ | CUSA06348 | Boots / logos only | [#1948](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1948) |
| The Amazing Spider-Man 2™ | CUSA00239 | Does not boot | [#2245](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2245) |
| The Art of Horizon Zero Dawn™ | CUSA07567 | Playable | [#2552](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2552) |
| The Art of Secret Ponchos | CUSA24508 | Ingame (playable-ish) | [#1281](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1281) |
| The Banner Saga | CUSA02538 | Ingame (playable-ish) | [#1949](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1949) |
| The Book of Unwritten Tales 2 | CUSA02025 | Playable | [#1215](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1215) |
| The Crew® 2 | CUSA08609 | Reaches menu | [#2164](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2164) |
| The Crew™ | CUSA00221 | Boots / logos only | [#140](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/140) |
| THE CREW™ MOTORFEST | CUSA26573 | Does not boot | [#2751](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2751) |
| The Dark Pictures Anthology: House Of Ashes | CUSA25846 | Boots / logos only | [#1950](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1950) |
| The Dark Pictures Anthology: Little Hope | CUSA18028 | Boots / logos only | [#1951](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1951) |
| The Dark Pictures Anthology: Man of Medan | CUSA13933 | Boots / logos only | [#1952](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1952) |
| The Dark Pictures Anthology: The Devil in Me | CUSA31496 | Boots / logos only | [#1953](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1953) |
| The Disney Afternoon Collection | CUSA06809 | Playable | [#1954](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1954) |
| The Elder Scrolls Online: Tamriel Unlimited | CUSA00086 | Reaches menu | [#2567](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2567) |
| The Evil Within | CUSA00203 | Boots / logos only | [#1955](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1955) |
| The Evil Within | CUSA00375 | Boots / logos only | [#1103](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1103) |
| The Evil Within® 2 | CUSA06166 | Boots / logos only | [#1956](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1956) |
| The First Tree | CUSA11699 | Playable | [#1453](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1453) |
| The Flame in the Flood | CUSA07205 | Ingame (playable-ish) | [#1957](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1957) |
| The House in Fata Morgana | CUSA15754 | Reaches menu | [#2221](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2221) |
| THE HOUSE OF THE DEAD: Remake | CUSA29480 | Reaches menu | [#2806](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2806) |
| The Illusionist-Andres Iniesta | CUSA10516 | Does not boot | [#1621](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1621) |
| The Inner World | CUSA07362 | Boots / logos only | [#1958](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1958) |
| The Inpatient | CUSA08981 | Boots / logos only | [#2025](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2025) |
| The Jackbox Party Pack | CUSA01229 | Reaches menu | [#1959](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1959) |
| The Last Guardian | CUSA04936 | Boots / logos only | [#227](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/227) |
| The Last Guardian VR DEMO | CUSA10479 | Ingame (playable-ish) | [#1264](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1264) |
| The Last Guardian™ | CUSA03745 | Ingame (playable-ish) | [#482](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/482) |
| The Last Guardian™ | CUSA03627 | Ingame (playable-ish) | [#350](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/350) |
| The Last of Us™ Part II | CUSA10249 | Does not boot | [#481](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/481) |
| The Last of Us™ Part II | CUSA07820 | Does not boot | [#141](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/141) |
| The Last of Us™ Remastered | CUSA00557 | Ingame (playable-ish) | [#2633](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2633) |
| The Last of Us™ Remastered | CUSA00552 | Reaches menu | [#142](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/142) |
| The Last of Us™ Remastered | CUSA00556 | Reaches menu | [#13](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/13) |
| The Last of Us™ Remastered | CUSA00559 | Does not boot | [#230](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/230) |
| THE LAST REMNANT Remastered | CUSA11846 | Ingame (playable-ish) | [#1795](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1795) |
| The Last Wind Monk | CUSA08876 | Playable | [#1960](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1960) |
| The Legend of Dragoon | CUSA32478 | Playable | [#2627](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2627) |
| The Legend of Heroes: Trails of Cold Steel | CUSA12471 | Ingame (playable-ish) | [#1835](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1835) |
| The Legend of Heroes: Trails of Cold Steel II | CUSA12472 | Ingame (playable-ish) | [#1839](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1839) |
| The LEGO® Movie - Videogame | CUSA00052 | Boots / logos only | [#2765](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2765) |
| The LEGO® NINJAGO® Movie Video Game | CUSA07744 | Playable | [#2565](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2565) |
| The Liar Princess and the Blind Prince | CUSA13189 | Playable | [#1454](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1454) |
| The Lord of the Rings: Gollum™ | CUSA33616 | Boots / logos only | [#2196](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2196) |
| The MISSING: J.J. Macfield and the Island of Memories, Demo Version | CUSA14318 | Playable | [#2563](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2563) |
| The Order: 1886 | CUSA00076 | Reaches menu | [#1339](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1339) |
| The Order: 1886 | CUSA00035 | Reaches menu | [#351](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/351) |
| The Order: 1886 | CUSA00100 | Boots / logos only | [#1328](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1328) |
| The Outer Worlds | CUSA16285 | Reaches menu | [#2680](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2680) |
| The Outer Worlds | CUSA13689 | Does not boot | [#2456](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2456) |
| The Pathless | CUSA14330 | Boots / logos only | [#2176](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2176) |
| The Persistence | CUSA07643 | Ingame (playable-ish) | [#1961](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1961) |
| THE PLAYROOM | CUSA00001 | Does not boot | [#143](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/143) |
| THE PLAYROOM VR | CUSA04318 | Does not boot | [#1623](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1623) |
| The Quarry | CUSA31820 | Does not boot | [#2065](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2065) |
| The Sims™ 4 | CUSA09216 | Reaches menu | [#479](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/479) |
| The Sinking City | CUSA13152 | Ingame (playable-ish) | [#1551](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1551) |
| The Sinking City | CUSA13337 | Does not boot | [#2511](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2511) |
| The Smurfs 2: The Prisoner of the Green Stone | CUSA43622 | Does not boot | [#1815](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1815) |
| The Surge 2 | CUSA12564 | Does not boot | [#1182](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1182) |
| The Unfinished Swan | CUSA00695 | Does not boot | [#1962](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1962) |
| The Walking Dead: Saints & Sinners | CUSA17412 | Does not boot | [#2026](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2026) |
| The Witch and the Hundred Knight 2 | CUSA10155 | Playable | [#2300](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2300) |
| The Witch and the Hundred Knight 2 | CUSA10135 | Playable | [#537](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/537) |
| The Witch and the Hundred Knight: Revival Edition | CUSA02799 | Does not boot | [#2303](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2303) |
| The Witch and the Hundred Knight: Revival Edition | CUSA02399 | Does not boot | [#240](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/240) |
| The Witcher 3: Wild Hunt | CUSA01439 | Ingame (playable-ish) | [#2447](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2447) |
| The Witcher 3: Wild Hunt | CUSA00527 | Boots / logos only | [#1963](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1963) |
| The Witcher 3: Wild Hunt – Game of the Year Edition | CUSA05573 | Ingame (playable-ish) | [#525](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/525) |
| The Witcher 3: Wild Hunt – Game of the Year Edition | CUSA05571 | Does not boot | [#472](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/472) |
| The Witness | CUSA04263 | Ingame (playable-ish) | [#2643](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2643) |
| The Wonderful 101: Remastered | CUSA18780 | Playable | [#2796](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2796) |
| Thief | CUSA00250 | Ingame (playable-ish) | [#2346](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2346) |
| Thief | CUSA00252 | Ingame (playable-ish) | [#1964](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1964) |
| Thief Demo | CUSA00652 | Ingame (playable-ish) | [#1417](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1417) |
| Tiny Tina's Wonderlands | CUSA23766 | Ingame (playable-ish) | [#377](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/377) |
| Titanfall™ 2 | CUSA04027 | Ingame (playable-ish) | [#346](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/346) |
| Tokyo 42 | CUSA07526 | Playable | [#1826](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1826) |
| TOKYO GHOUL:re CALL to EXIST | CUSA12474 | Ingame (playable-ish) | [#2060](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2060) |
| TOKYO GHOUL:re CALL to EXIST | CUSA12452 | Ingame (playable-ish) | [#144](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/144) |
| Tokyo Xanadu eX+ | CUSA06987 | Ingame (playable-ish) | [#1800](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1800) |
| Tokyo Xanadu eX+ | CUSA06978 | Ingame (playable-ish) | [#1359](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1359) |
| Tom Clancy's Ghost Recon® Wildlands | CUSA02902 | Boots / logos only | [#2636](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2636) |
| Tom Clancy's Ghost Recon® Wildlands | CUSA02821 | Boots / logos only | [#2370](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2370) |
| Tom Clancy's Ghost Recon® Wildlands | CUSA02819 | Boots / logos only | [#1198](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1198) |
| Tom Clancy’s Rainbow Six® Extraction | CUSA15434 | Does not boot | [#363](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/363) |
| Tom Clancy's Rainbow Six® Siege | CUSA02368 | Does not boot | [#88](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/88) |
| Tom Clancy's The Division™ | CUSA01810 | Boots / logos only | [#2771](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2771) |
| Tom Grennan VR | CUSA13051 | Does not boot | [#1624](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1624) |
| Tomb Raider I-III Remastered Starring Lara Croft | CUSA43773 | Playable | [#2301](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2301) |
| Tomb Raider IV-VI Remastered | CUSA44837 | Playable | [#2296](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2296) |
| Tomb Raider: Definitive Edition | CUSA00109 | Playable | [#1901](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1901) |
| Tomb Raider: Definitive Edition | CUSA00107 | Playable | [#769](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/769) |
| Tony Hawk's™ Pro Skater™ 1 + 2 | CUSA17922 | Boots / logos only | [#1180](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1180) |
| Tony Hawk's™ Pro Skater™ 3 + 4 | CUSA44732 | Boots / logos only | [#2297](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2297) |
| Toukiden: Kiwami | CUSA02002 | Ingame (playable-ish) | [#1040](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1040) |
| Tower of Fantasy | CUSA35049 | Reaches menu | [#1625](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1625) |
| TowerFall Ascension | CUSA00466 | Playable | [#2529](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2529) |
| Trackmania | CUSA25218 | Ingame (playable-ish) | [#1282](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1282) |
| Trackmania Turbo | CUSA03008 | Ingame (playable-ish) | [#2679](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2679) |
| Trackmania Turbo | CUSA01210 | Ingame (playable-ish) | [#401](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/401) |
| TRANSFORMERS: Devastation | CUSA01410 | Ingame (playable-ish) | [#1351](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1351) |
| Trials Fusion™ | CUSA00230 | Does not boot | [#2165](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2165) |
| Trials Fusion™ | CUSA00304 | Does not boot | [#145](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/145) |
| Trials of Mana | CUSA16978 | Ingame (playable-ish) | [#2647](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2647) |
| Tricky Towers | CUSA05198 | Reaches menu | [#2422](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2422) |
| Trine 2: Complete Story | CUSA00234 | Playable | [#1965](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1965) |
| Trine 3: The Artifacts of Power | CUSA03082 | Ingame (playable-ish) | [#1967](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1967) |
| Trine 4: The Nightmare Prince | CUSA10755 | Does not boot | [#1968](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1968) |
| Trine Enchanted Edition | CUSA01135 | Playable | [#1969](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1969) |
| TROVE | CUSA05221 | Reaches menu | [#1418](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1418) |
| Trover Saves the Universe | CUSA11065 | Ingame (playable-ish) | [#887](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/887) |
| TubiTV | CUSA08686 | Does not boot | [#1627](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1627) |
| Twitch | CUSA03398 | Does not boot | [#2553](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2553) |
| Twitch | CUSA03285 | Does not boot | [#1628](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1628) |
| Two Point Hospital | CUSA15884 | Playable | [#334](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/334) |
| UEFA.tv | CUSA25007 | Does not boot | [#1629](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1629) |
| UFC Fight Pass | CUSA35488 | Does not boot | [#1630](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1630) |
| ULTIMATE MARVEL VS. CAPCOM 3 | CUSA04622 | Ingame (playable-ish) | [#1155](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1155) |
| Unbox: Newbie's Adventure | CUSA07938 | Ingame (playable-ish) | [#1120](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1120) |
| Uncharted 4: A Thief’s End™ | CUSA00341 | Boots / logos only | [#146](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/146) |
| Uncharted: The Lost Legacy™ | CUSA07737 | Boots / logos only | [#341](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/341) |
| Uncharted: The Nathan Drake Collection™ | CUSA02320 | Ingame (playable-ish) | [#147](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/147) |
| Uncharted: The Nathan Drake Collection™ | CUSA01399 | Does not boot | [#267](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/267) |
| Uncharted™ 2: Among Thieves Remastered | CUSA03281 | Ingame (playable-ish) | [#2689](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2689) |
| Uncharted™ 4: A Thief’s End | CUSA04529 | Boots / logos only | [#2634](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2634) |
| Uncharted™ 4: A Thief’s End | CUSA00918 | Boots / logos only | [#2233](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2233) |
| Uncharted™ 4: A Thief’s End | CUSA00917 | Does not boot | [#478](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/478) |
| Uncharted™: The Lost Legacy | CUSA07875 | Boots / logos only | [#1497](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1497) |
| Uncharted™: The Nathan Drake Collection | CUSA02343 | Reaches menu | [#2645](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2645) |
| Uncharted™: The Nathan Drake Collection | CUSA02344 | Reaches menu | [#441](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/441) |
| Uncharted™: The Nathan Drake Collection | CUSA02826 | Boots / logos only | [#10](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/10) |
| Uncharted™: The Nathan Drake Collection Demo | CUSA03331 | Boots / logos only | [#1419](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1419) |
| Undertale | CUSA09415 | Playable | [#2777](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2777) |
| Unearthing Mars | CUSA07655 | Does not boot | [#2186](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2186) |
| UNO® | CUSA04040 | Playable | [#1402](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1402) |
| Unravel | CUSA02532 | Playable | [#1980](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1980) |
| Unravel TWO | CUSA10416 | Reaches menu | [#1992](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1992) |
| Until Dawn™ | CUSA02636 | Ingame (playable-ish) | [#617](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/617) |
| Until Dawn™ | CUSA00359 | Reaches menu | [#679](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/679) |
| Until Dawn™ | CUSA00194 | Boots / logos only | [#2616](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2616) |
| Until Dawn™: Rush of Blood | CUSA03683 | Does not boot | [#2027](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2027) |
| Untitled Goose Game | CUSA23079 | Playable | [#927](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/927) |
| Valhalla Hills | CUSA05718 | Ingame (playable-ish) | [#2109](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2109) |
| Valkyria Revolution | CUSA06995 | Playable | [#512](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/512) |
| VALKYRIE ELYSIUM | CUSA33214 | Boots / logos only | [#2148](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2148) |
| Vampire the Masquerade : Swansong | CUSA33241 | Reaches menu | [#1721](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1721) |
| Vanquish | CUSA16161 | Ingame (playable-ish) | [#2808](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2808) |
| VARIOUS DAYLIFE | CUSA33148 | Reaches menu | [#2118](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2118) |
| Velocity®2X | CUSA07337 | Playable | [#712](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/712) |
| War Thunder | CUSA00224 | Does not boot | [#1632](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1632) |
| Warface | CUSA12894 | Boots / logos only | [#1420](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1420) |
| Warframe | CUSA00080 | Does not boot | [#1631](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1631) |
| Watch Dogs®: Legion | CUSA13034 | Does not boot | [#2122](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2122) |
| WATCH_DOGS® 2 | CUSA04459 | Reaches menu | [#1633](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1633) |
| WATCH_DOGS® 2 | CUSA04294 | Boots / logos only | [#386](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/386) |
| WATCH_DOGS® 2 | CUSA04295 | Does not boot | [#1421](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1421) |
| WATCH_DOGS™ | CUSA00016 | Ingame (playable-ish) | [#1422](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1422) |
| WE ARE DOOMED | CUSA01783 | Playable | [#1634](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1634) |
| WeatherNation | CUSA03976 | Does not boot | [#1635](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1635) |
| What Remains of Edith Finch | CUSA06886 | Ingame (playable-ish) | [#1993](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1993) |
| Wild Arms 2 | CUSA34856 | Playable | [#2646](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2646) |
| WIPEOUT™ OMEGA COLLECTION | CUSA07671 | Ingame (playable-ish) | [#1994](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1994) |
| WIPEOUT™ OMEGA COLLECTION | CUSA05670 | Ingame (playable-ish) | [#487](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/487) |
| Wizard of Legend | CUSA10927 | Ingame (playable-ish) | [#1452](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1452) |
| Wolfenstein®: The New Order | CUSA00314 | Reaches menu | [#1096](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1096) |
| Wonder Boy Anniversary Collection | CUSA32586 | Playable | [#847](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/847) |
| Wonder Boy: Asha in monster world | CUSA26735 | Reaches menu | [#1380](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1380) |
| WORLD OF FINAL FANTASY | CUSA04647 | Playable | [#354](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/354) |
| World Of Warriors | CUSA04098 | Ingame (playable-ish) | [#2078](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2078) |
| World to the West | CUSA06930 | Playable | [#2085](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2085) |
| Worms Battlegrounds | CUSA00192 | Playable | [#268](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/268) |
| Wuppo | CUSA08517 | Playable | [#1715](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1715) |
| WWE 2K17 | CUSA05058 | Ingame (playable-ish) | [#2635](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2635) |
| YAKUZA 0 | CUSA05070 | Playable | [#2002](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2002) |
| YAKUZA 0 | CUSA05133 | Ingame (playable-ish) | [#1656](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1656) |
| YAKUZA 3 | CUSA15360 | Ingame (playable-ish) | [#2648](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2648) |
| YAKUZA 3 | CUSA15325 | Boots / logos only | [#1999](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1999) |
| YAKUZA 4 | CUSA15361 | Ingame (playable-ish) | [#2652](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2652) |
| YAKUZA 4 | CUSA15326 | Boots / logos only | [#2000](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2000) |
| YAKUZA 5 | CUSA15363 | Ingame (playable-ish) | [#2418](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2418) |
| YAKUZA 5 | CUSA15327 | Boots / logos only | [#2001](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2001) |
| YAKUZA 6: The Song of Life | CUSA09660 | Does not boot | [#2154](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2154) |
| YAKUZA 6: The Song of Life | CUSA09032 | Does not boot | [#1067](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1067) |
| YAKUZA KIWAMI | CUSA06997 | Playable | [#1066](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1066) |
| YAKUZA KIWAMI | CUSA07615 | Ingame (playable-ish) | [#2653](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2653) |
| YAKUZA KIWAMI 2 | CUSA10706 | Ingame (playable-ish) | [#2654](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2654) |
| YAKUZA KIWAMI 2 | CUSA10634 | Does not boot | [#1063](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1063) |
| Yakuza: Like A Dragon | CUSA16745 | Does not boot | [#1850](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1850) |
| Yonder: The Cloud Catcher Chronicles | CUSA08242 | Playable | [#1998](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1998) |
| Yooka-Laylee | CUSA05751 | Playable | [#912](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/912) |
| Yooka-Laylee and the Impossible Lair | CUSA16139 | Playable | [#149](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/149) |
| Yooka-Laylee and the Impossible Lair | CUSA16148 | Ingame (playable-ish) | [#1997](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1997) |
| Yooka-Laylee Toybox Demo | CUSA07093 | Playable | [#1996](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1996) |
| YouTube | CUSA01116 | Ingame (playable-ish) | [#2163](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2163) |
| YouTube | CUSA01015 | Boots / logos only | [#1636](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1636) |
| YouTube TV | CUSA18680 | Boots / logos only | [#1637](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1637) |
| Ys IX: Monstrum Nox | CUSA20413 | Ingame (playable-ish) | [#1806](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1806) |
| Ys IX: Monstrum Nox | CUSA20414 | Ingame (playable-ish) | [#1376](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1376) |
| Ys VIII -Lacrimosa of DANA- | CUSA08570 | Ingame (playable-ish) | [#1805](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1805) |
| Ys VIII -Lacrimosa of DANA- | CUSA08565 | Boots / logos only | [#1450](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1450) |
| Ys X: Nordics | CUSA47530 | Ingame (playable-ish) | [#1813](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1813) |
| Ys: Memories of Celceta | CUSA18056 | Ingame (playable-ish) | [#1825](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1825) |
| Yu-Gi-Oh! Master Duel | CUSA24659 | Does not boot | [#1638](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1638) |
| Zero Strain | CUSA18570 | Ingame (playable-ish) | [#967](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/967) |
| ZOMBI | CUSA03535 | Playable | [#1995](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1995) |
| Zombie Driver: Immortal Edition | CUSA17453 | Playable | [#1819](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1819) |
| Zombieland: Double Tap - Road Trip | CUSA15243 | Ingame (playable-ish) | [#355](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/355) |
| Zombies Ate My Neighbors and Ghoul Patrol | CUSA18841 | Playable | [#303](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/303) |
| ZONE OF THE ENDERS THE 2nd RUNNER : M∀RS | CUSA10631 | Ingame (playable-ish) | [#1368](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1368) |
| アイドルマスター ステラステージ | CUSA07753 | Ingame (playable-ish) | [#836](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/836) |
| アリス・ギア・アイギスCS | CUSA32352 | Ingame (playable-ish) | [#2479](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2479) |
| ガンダムブレイカー４ | CUSA34133 | Ingame (playable-ish) | [#2476](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2476) |
| とある魔術の電脳戦機 | CUSA08425 | Reaches menu | [#255](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/255) |
| ノラと皇女と野良猫ハート2 | CUSA13586 | Playable | [#1398](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1398) |
| ハコニワカンパ���ワークス | CUSA08248 | Ingame (playable-ish) | [#513](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/513) |
| ひ���らしのなく頃に奉 | CUSA13343 | Playable | [#2130](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2130) |
| フユキス | CUSA29745 | Playable | [#1572](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1572) |
| ペルソナ３ リロード | CUSA24782 | Reaches menu | [#2481](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2481) |
| ペルソナ５ ザ・ロイヤル | CUSA08644 | Boots / logos only | [#2133](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2133) |
| ユニコーンオーバーロード | CUSA27714 | Playable | [#852](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/852) |
| 仮面ライダー バトライド・ウォー ���生 | CUSA02714 | Playable | [#2503](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2503) |
| 初音ミク -Project DIVA- X HD | CUSA02730 | Boots / logos only | [#2201](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2201) |
| 初音ミク Project DIVA Future Tone DX | CUSA08026 | Playable | [#1722](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1722) |
| 十三機兵防衛圏 プロローグ | CUSA14276 | Reaches menu | [#863](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/863) |
| 実況パワフルプロ野球２０１６ | CUSA03231 | Does not boot | [#2170](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2170) |
| 春音アリス*グラム | CUSA14324 | Playable | [#1653](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1653) |
| 東方深秘録 | CUSA06012 | Playable | [#2432](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2432) |
| 海腹川背 Fresh! | CUSA17449 | Ingame (playable-ish) | [#2406](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2406) |
| 絶体絶命都市4Plus -Summer Memories- | CUSA06589 | Ingame (playable-ish) | [#2641](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2641) |
| 零 ～濡鴉之巫女～ | CUSA28987 | Reaches menu | [#316](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/316) |
| 電車でＧＯ！！ はしろう山手線 | CUSA19171 | Ingame (playable-ish) | [#1720](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/1720) |

<!--
Row template (copy/paste and fill in from the issue). Use a full issue URL, not a relative link:
| Game Name | CUSAxxxxx | Ingame (playable-ish) | v1.3 | [#123](https://github.com/Coder787-source/KytyPlus/issues/123) |
-->
