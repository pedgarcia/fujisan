# MCP Server Vision for Fujisan

**Status**: Planning / Science Project
**Date**: 2025-10-29
**Purpose**: Brainstorming document for integrating Fujisan with MCP to enable AI-assisted emulator control

## The Problem

To create an effective MCP server that allows Claude to control and interact with the Atari 800 emulator, we need to solve: **How does Claude "see" what's on the screen?**

Different Atari display modes present different challenges and opportunities:
- **Character/Text Mode**: 960 bytes of screen RAM (40x24), ATASCII characters
- **Mixed Mode**: Text + graphics regions combined
- **Full Bitmap Mode**: 8-16KB of graphics memory, multiple ANTIC modes

## Technical Approaches Considered

### Character Mode Options

**Screen Memory Dump**
- Dump 960 bytes of screen RAM + color registers + charset info
- Pros: Tiny (~1KB), direct text interpretation, very fast
- Cons: Need to handle inverse video, custom charsets

### Mixed Mode Options

**Hybrid Approach**
- Text area: memory dump
- Graphics area: bitmap or screenshot region
- Pros: Efficient for text portion
- Cons: Complex, need display list parsing

### Bitmap Mode Options

**Screenshot (PCX/PNG)**
- Use existing screenshot capability
- Pros: AI can "see" directly, already implemented, works for everything
- Cons: Large files (10-50KB), transfer overhead

**Raw Bitmap + Palette**
- Dump screen memory (8-16KB) + color registers
- Pros: More efficient than image files, precise pixel data
- Cons: Still significant data, need mode understanding

## Recommended Approach: Hybrid with Adaptive Detail

### Command 1: `screen.state` - Fast Status Polling

Lightweight status for change detection:

```json
{
  "mode": "text|mixed|graphics",
  "display_mode": 2,
  "text_rows": 24,
  "text_cols": 40,
  "cursor": {"x": 5, "y": 10, "visible": true},
  "hash": "abc123..."
}
```

**Purpose**: Fast polling for gameplay (can poll at 60Hz)
**Size**: ~100 bytes

### Command 2: `screen.read [detail_level]` - Content Retrieval

**Level 0 - Text Only (~1KB)**:
- Screen RAM as ATASCII/readable text
- Colors
- Perfect for programming/typing scenarios

**Level 1 - Text + Metadata (~2KB)**:
- Screen codes (raw values)
- Character set info
- ANTIC display list summary
- Player/missile positions

**Level 2 - Full Visual (~8-20KB)**:
- For graphics modes: raw bitmap + palette OR small PNG screenshot
- When AI needs to "see" what's happening

## Implementation Phases

### Phase 1: Enhanced Screenshots + Metadata ⭐ START HERE

**Goal**: Get something working quickly for experimentation

**Implementation**:
- Add `screen.shot json` command returning:
  - Base64-encoded PNG screenshot
  - Display mode information
  - Cursor position
  - Current memory/state metadata

**Client Simplicity**:
- Just parse JSON + base64 image (trivial)
- Claude's vision model handles all display modes
- No need to understand Atari-specific formats initially

**Use Cases Enabled**:
- All interactive use cases (see below)
- Good enough for science project phase
- Learn what actually matters through use

### Phase 2: Text Mode Optimization

**Goal**: Add bandwidth efficiency for text-heavy scenarios

**Add**: If text mode detected, include character array in response
- MCP client can choose: fast text parsing vs. vision
- Saves bandwidth for programming/typing use cases
- Backward compatible with Phase 1

### Phase 3: Real-Time Gaming Support

**Goal**: Optimize for gameplay scenarios

**Add**:
- `screen.state` hash/change detection
- Player/sprite position APIs
- Frame-by-frame control APIs
- Minimize latency for action games

## Use Cases

### 1. Interactive Programming Tutor ⭐

**Scenario**: "Teach me BASIC programming on an Atari 800"

- AI sees the screen, guides user through typing programs
- When syntax errors occur, AI sees error message and explains
- Can paste code snippets via TCP
- Could type in example programs and run them
- **Value**: Learning vintage programming with AI assistance

**Example Session**:
```
User: "Help me write a BASIC program that draws a rainbow"
Claude:
- Takes screenshot, sees "READY" prompt
- Types: 10 GRAPHICS 7
- Takes screenshot, sees graphics mode
- Types: 20 COLOR 1
- Continues building program line by line
- Types: RUN
- Sees result, adjusts colors
- "Here's your rainbow! Want to add animation?"
```

### 2. Automated Software Testing

**Scenario**: "Test this disk image and tell me if the program works"

- Load disk image via TCP
- Navigate menus by seeing what's on screen
- Try different inputs based on visual feedback
- Report bugs, crashes, unexpected behavior
- Build regression test suites for homebrew software
- **Value**: Practical testing tool for Atari homebrew developers

### 3. Game Strategy Helper ⭐

**Scenario**: "Help me get past this level in Adventure"

- See the game screen
- Analyze player position, enemy patterns
- Suggest moves ("move left, the dragon is approaching from the right")
- Not fast enough for real-time action games
- Perfect for adventure games, puzzles, strategy games
- **Value**: AI co-pilot for retro gaming

### 4. Historical Software Archeology

**Scenario**: "What does this mysterious disk image do?"

- Load unknown software
- Explore menus, try inputs
- Document what the software does
- Take notes, create manual/documentation
- Catalog old software libraries
- **Value**: Preserve knowledge about obscure software

### 5. Assisted Retro Development

**Scenario**: "I'm writing an Atari game, help me test it"

Developer workflow:
- Build game on modern machine
- Auto-load into Fujisan via TCP
- AI plays/tests it, sees bugs
- Reports issues: "The sprite at row 15 is glitching"
- Fast iteration cycle
- **Value**: Make retro development less tedious

### 6. Interactive Demos / Livestream Assistant

**Scenario**: "Show me what the Atari can do"

- AI controls the emulator
- Gives narration: "Now I'm loading a graphics demo..."
- Shows off different software
- Explains what's happening technically
- **Value**: Automated retro computing content creation

### 7. Educational Scenarios

**Scenario**: "Demonstrate how the Atari file system works"

- Load DOS
- Show directory listing
- Explain what AI is seeing
- Run commands, show results
- Interactive computer history lessons
- **Value**: Make vintage computing accessible

### 8. Debugging Assistant for Developers

**Scenario**: "My program crashes at line 250, help me debug"

- Load user's program
- Run with debugger
- See the crash
- Examine memory
- Read screen state and debug output
- Suggest fixes based on observations
- **Value**: Vintage debugger with AI assistance

## Streaming and Autonomy: MCP Limitations

### The Fundamental Constraint

**MCP + Claude Desktop Architecture**:
- Claude only "wakes up" when user sends a message
- Can call MCP tools during response
- Then goes back to sleep
- **Cannot** run in background or monitor streams

**Each interaction is**:
1. User sends message
2. Claude responds (using MCP tools)
3. Done - Claude stops

**Claude Cannot**:
- Subscribe to a stream of screenshots
- Proactively comment ("Hey! You missed that enemy!")
- Monitor in background and interrupt user
- Run autonomous loops

### Option 1: Semi-Autonomous (Within MCP)

User: "Watch me play for the next 2 minutes and give tips"

```
Loop 60 times:
  - Call screen.shot via MCP
  - Wait 2 seconds
  - Collect screenshot
End loop
Analyze all screenshots
Give summary
```

**Limitations**:
- User waits while this runs (blocking)
- No mid-game interruptions
- Single request-response, not true streaming

### Option 2: Custom Agent Program (True Streaming)

Separate program that bridges Fujisan TCP ↔ Claude API:

```javascript
const agent = new ClaudeAgent({
  pollingInterval: 1000,
  fujisanHost: 'localhost:6502'
});

agent.on('screen-change', async (screenshot) => {
  const analysis = await claude.analyze(screenshot);

  if (analysis.hasInterestingObservation) {
    notify.user(analysis.observation);
  }
});

agent.start();
```

**Pros**:
- Real streaming experience
- AI can "interrupt" with observations
- Works for gameplay, tutoring, debugging
- Truly asynchronous

**Cons**:
- Need to write custom program
- Uses Claude API (costs per call)
- Not integrated with MCP/Claude Desktop

### Option 3: Interactive Polling ⭐ RECOMMENDED START

**For programming tutoring**:
- User types in emulator
- User: "Check my code"
- Claude: Screenshot → analysis → feedback

**For gameplay**:
- User: "What should I do?" (when stuck)
- Claude: Screenshot → strategic advice

**For debugging**:
- User: "What's wrong?" (after crash)
- Claude: Screenshot + debug state → diagnosis

**Why Start Here**:
- Works today with MCP, no custom code
- Good for science project phase
- Learn what kind of streaming you actually want
- Can build Option 2 later with better understanding

## Implementation Strategy

### Near Term (Science Project Phase)

1. **Implement Phase 1**: Enhanced screenshots + metadata
   - Add `screen.shot json` TCP command
   - Returns base64 PNG + metadata JSON
   - Simple, works for all use cases

2. **Build Basic MCP Server**
   - Wraps existing TCP commands
   - Adds new `screen.shot` command
   - Focus on simplicity

3. **Experiment with Use Cases**
   - Try Programming Tutor scenarios
   - Try Game Strategy scenarios
   - Learn what works, what's missing

### Future Enhancements

**When text mode proves important**:
- Implement Phase 2 (text optimization)
- Reduce bandwidth for programming scenarios

**When gameplay becomes compelling**:
- Implement Phase 3 (real-time optimization)
- Add change detection, state APIs

**When autonomous behavior is needed**:
- Build custom agent (Option 2)
- Bridge Fujisan TCP ↔ Claude API
- Enable true streaming/interruption

## Technical Notes

**Both MCP and Custom Agent Can Coexist**:
- MCP server gives you the tools (`screen.shot`, `input.type`, etc.)
- Use MCP interactively for "on-demand" help
- Separate agent program uses same TCP server for autonomous monitoring
- Not mutually exclusive

**Efficiency vs. Simplicity Trade-off**:
- Phase 1 prioritizes simplicity (screenshots work for everything)
- Can add efficiency later (text dumps, change detection)
- Don't optimize prematurely - learn what matters first

## Next Steps

When ready to implement:

1. Review existing TCP server commands in Fujisan
2. Add `screen.shot json` command (Phase 1)
3. Create simple MCP server wrapper
4. Test with Programming Tutor scenario
5. Document findings, iterate on design
6. Consider Phase 2/3 based on learnings

---

*This is a living document. Update as we learn more through experimentation.*
