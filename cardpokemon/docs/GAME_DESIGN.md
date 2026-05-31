# CardPokemon Game Design Bible

Version: 0.1  
Target hardware: M5Stack Cardputer ADV + Cap LoRa-1262/GPS  
Design goal: 一个可以随车自动游玩的宝可梦式驾驶冒险游戏。设备启动后只服务于这一个游戏，核心乐趣来自真实移动、宝可梦图像/GIF、捕获、养成、图鉴收集和短决策。

## 1. 核心概念

CardPokemon 是一个驾驶驱动的宝可梦收集 RPG。玩家先从三只 1 级初始宝可梦中选择伙伴，然后在开车、步行或携带设备移动时自动探索。GPS 定位和真实移动决定是否能够遇敌以及累计探索距离，6 轴 IMU 的相对运动热度影响野外宝可梦等级、稀有度和遇敌间隔。

游戏不要求玩家在驾驶中操作。行驶时主界面只展示伙伴、距离、速度、GPS 状态和自动事件。停车或手动进入菜单时，玩家可以查看图鉴、切换伙伴、治疗、购买道具、整理背包和准备下一段旅程。

基调不是完整复刻主线宝可梦，而是“小屏驾驶宝可梦”：轻量、自动、图像优先、数值清楚、随时可停。

## 2. 设计原则

- 不要求驾驶时交互。
- 第一眼必须看到当前伙伴和状态。
- 图片/GIF 是核心体验，不是装饰。
- 行驶产生冒险，停车产生决策。
- 数值系统要短小，适合 240x135 屏幕。
- 玩法深度来自捕获、进化、队伍、道具、商店、路线和图鉴，而不是复杂 UI。
- 所有 SD 资源都必须可缺省：没有 SD 卡也能进入内置 fallback。

## 3. 核心循环

1. 选择伙伴
   - 首次启动进入初始选择。
   - 默认可选 Bulbasaur、Charmander、Squirtle。
   - 三只初始宝可梦都会登记为已拥有，当前选择作为出行伙伴。

2. 出行探索
   - GPS 距离累计探索进度。
   - 只有 GPS fresh 且发生真实位移时才推进遇敌。
   - IMU 运动强度提供活动热度。
   - 热度越高，下一次遇敌所需距离越短，野外宝可梦越可能稀有或等级更高。

3. 自动战斗
   - 根据伙伴等级、属性、基础能力、运动加成和随机数判定胜负。
   - 战斗期间不用操作。
   - 胜利获得 XP 和金币，极小概率捕获对方。

4. 奖励与成长
   - 伙伴获得 XP。
   - 满 XP 后升级，HP 回复，能力提高。
   - 达到进化等级且进化目标存在于 manifest 时进化。

5. 停车整备
   - 进入营地/中心/商店类页面。
   - 治疗、买道具、卖掉材料、切换伙伴、查看图鉴。

6. 扩展收集
   - 捕获更多宝可梦。
   - 图鉴条目从“见过”变成“捕获”。
   - 用不同伙伴继续探索更远路线。

## 4. 硬件输入到游戏意义

| 输入 | 游戏意义 | 第一版用途 | 后续用途 |
|---|---|---|---|
| GPS 距离 | 真实探索进度 | 推动遇敌和总距离 | 路线、地区、任务 |
| GPS 速度 | 行驶节奏 | 显示和路线状态 | 城市/高速/步行状态 |
| GPS 卫星数 | 定位可靠性 | 显示 | 稀有事件或迷路事件 |
| GPS HDOP | 定位质量 | 过滤漂移 | 路线质量评分 |
| IMU 加速度 | 相对运动强度 | 遇敌等级/稀有度/间隔 | 道路状态、活动奖励 |
| IMU 陀螺仪 | 转向/震动 | motionScore | 急转、颠簸、危险事件 |
| SD 卡 | 资源库 | PNG/GIF/manifest | 音效、地图、活动包 |
| LoRa | 附近信号 | 暂缓 | 附近训练师、交换、救援 |

## 5. 场景结构

### Boot

- 初始化 HAL、显示、键盘、IMU、Cap GPS、SD。
- 读取 `/sdcard/manifest.json`。
- 读取 NVS 存档。
- 显示资源状态：SD、manifest、PNG 数量。

### Starter Select

- 三列卡片：图像、名字、属性。
- 左右选择，Enter 确认。
- 确认后进入 Travel。

### Travel

主屏幕需要是游戏的“仪表盘”。

建议布局：

```text
CardPokemon
[64x64 partner]  Pikachu
                 Lv7 Electric
                 HP 42/48
                 XP 18/126
Dist 4.2km       [wild activity waveform]
36.5kmh S9 H1.2 M0.8
```

规则：

- 不超过六到七行有效信息。
- 不显示长说明。
- 事件反馈替换底部状态，不新增拥挤文本。

### Battle

- 左侧野外宝可梦，右侧玩家伙伴。
- 显示双方名字和等级。
- 自动 resolving，约 1.4 秒后出结果。
- 结果：胜利、失败、捕获、XP。

### Pokedex / Party

- 图鉴浏览和队伍选择合并。
- 默认进入 2x4 小图网格，一页显示 8 只宝可梦。
- 网格显示固件内置重点色徽章/问号、全国编号、Seen/Caught 状态边框。
- 按 Enter 进入详情页，详情页左侧图像，右侧名字、属性、捕获状态、等级/HP/XP。
- 已捕获才能设为当前伙伴。
- 未捕获可以显示“SEEN?”，后续可加剪影。

### Camp / Center

停车或菜单进入。相当于宝可梦中心 + 背包 + 商店的轻量组合。

初版页面：

```text
Camp Center
HP 28/48  Gold 36
Pot 2 Ball 4
> Heal team
  Shop
  Bag
  Leave
```

## 6. 宝可梦数据模型

来自 manifest 的字段：

| 字段 | 用途 |
|---|---|
| `id` | 全国图鉴编号、资源文件名 |
| `name` | 显示名 |
| `types[]` | 属性，第一属性用于颜色和简化克制 |
| `stats.hp` | 转换为基础 HP |
| `stats.attack` | 转换为 ATK |
| `stats.defense` | 转换为 DEF |
| `png` | 64x64 静态图 |
| `gif` | 后续动画源 |
| `game.catch_rate` | 捕获率基础 |
| `game.evolve_level` | 进化等级 |
| `game.evolve_to` | 进化目标 id |

固件内运行时结构建议：

```text
Species:
  id, name, type, baseHp, atk, def
  catchRate, evolveLevel, evolveTo
  color

PokemonState:
  owned, seen
  level, xp, hp
  nickname optional
```

第一版不需要性格、个体值、努力值、招式 PP。Cardputer 屏幕太小，先保留“伙伴能力 + 自动战斗”的清晰感。

## 7. 属性系统

第一阶段使用单属性简化克制。

| 攻击属性 | 强势 | 弱势 |
|---|---|---|
| Fire | Grass, Bug, Ice | Water, Rock |
| Water | Fire, Rock, Ground | Grass, Electric |
| Grass | Water, Rock, Ground | Fire, Bug |
| Electric | Water, Flying | Grass, Ground |
| Flying | Grass, Bug | Electric, Rock |
| Normal | none | Rock |
| Poison | Grass | Ground |
| Psychic | Poison | Dark |
| Rock | Fire, Flying, Bug | Water, Grass |
| Ground | Fire, Electric, Poison | Water, Grass |

简化公式：

```text
typeBonus = +4 if strong
typeBonus = -3 if weak
otherwise 0
```

第一版可以先只实现 Fire/Water/Grass/Electric/Flying/Normal，后续再补齐全部类型。

## 8. 等级与 XP

目标：短途也有反馈，长途不爆炸。

推荐 XP 曲线：

```text
xp_to_next = 14 + level * 16 + floor(level * level / 3)
```

当前固件的简单公式 `14 + level * 16` 适合早期版本，后续可换成上面的微曲线。

升级奖励：

- Max HP 增加。
- 当前 HP 回满。
- 战斗 power 自然随 level 提升。
- 每 5 级可解锁一条小被动，后续再做。

推荐等级范围：

| 阶段 | 等级 | 体验 |
|---|---:|---|
| 初期 | 1-10 | 快速熟悉，常见宝可梦 |
| 中期 | 11-30 | 进化、商店、更多区域 |
| 后期 | 31-60 | 稀有宝可梦、路线任务 |
| 长线 | 61-99 | 收集和自我挑战 |

## 9. 遇敌系统

### 野外活跃度

```text
encounterHeat += motionGain + gpsMovingGain + itemGain - decay
```

`encounterHeat` 是 0-100 的野外活跃度。它不是固定距离进度条，而是“附近越来越有动静”的活动压力。主界面用一条冷到热的示波线表现活跃度，不直接显示百分比。

遇敌门槛由 GPS 和 IMU 分工：

- GPS fresh 且检测到真实位移，才允许遇敌。
- 真实 GPS 位移累计 `distanceSinceEventM`。
- IMU 热度不直接伪造距离，只影响下一次遇敌目标距离、野怪稀有度和等级。
- 没有 GPS 位移时，即使持续摇晃设备，也不会触发遇敌。
- GPS 速度会改变 IMU 的解释方式：走路时 IMU 主要提升活跃感，行车时 IMU 才明显提高稀有度和等级压力。

增长因素：

- motionScore 高：热度增长，且影响野怪等级/稀有度。
- GPS fresh 且正在移动：热度小幅增长，同时累计遇敌距离。
- Lure：提升热度并缩短下一次遇敌目标距离。
- Repel：降低热度并拉长下一次遇敌目标距离。

速度模式：

- Walk：约 1-7km/h。手持摆动会提高遇怪活跃感，但等级加成最多约 +1。
- City/Bike：约 7-18km/h。IMU 有中等影响，等级压力最多约 +2。
- Drive：18km/h 以上。IMU 更接近路况/驾驶强度，允许更高稀有度和等级压力。
- Rough：18km/h 以上且 IMU 明显颠簸时进入，适合触发强度更高的行车探索。

推荐：

```text
if cooldown over and gpsFresh and gpsStep:
  if distanceSinceEventM >= nextEncounterTargetM:
    start encounter
```

目标距离：

- 遇敌后有 45 秒冷却。
- 低热度约 150-260m。
- 中热度约 115-210m。
- 高热度约 80-155m。
- 极高热度约 55-115m。
- 步行和颠簸路线略短，高速路线略长。

当前实现注意点：

- 固件没有模拟 GPS、模拟距离或按键强制遇怪。
- GPS 真实经纬度通过 `haversine_m()` 转换成距离，并通过最小步长和速度相关最大步长过滤。
- IMU 只影响热度和战斗/遇怪压力，不会在无 GPS 位移时触发遇怪。
- 走路模式下热度仍可提升遇怪频率，但稀有度和等级压力会被限幅，避免普通步行导致野怪等级过高。
- GPS stale 时速度显示会衰减，但这只是 UI 平滑，不会增加距离。
- 仍需外场观察 GPS 漂移：如果静止时坐标漂移超过过滤阈值，可能产生少量距离。

### 野怪池解锁

```text
maxDexId = 35 + totalDistanceM / 180 + activeLevel * 3 + encounterHeat * 0.45
```

含义：

- 刚开始主要遇到编号较靠前、基础宝可梦。
- 行驶越远，候选池越大。
- 当前伙伴等级越高，能遇到更强/更后段编号。
- IMU 热度高时，会有更大概率越级遇到稀有目标。

### 野怪等级

```text
wildLevel = activeLevel + distanceBonus + motionBonus + random(-2..1)
distanceBonus = clamp(totalDistanceM / 2500 + sessionDistanceM / 1200, 0, 35)
motionBonus = clamp(encounterHeat / 28, 0, 4)
```

这样可以让长途旅行自然变危险，持续活跃路段更容易出现强敌，但不会因为瞬间震动一下直接崩坏。

## 10. 战斗系统

战斗自动结算，避免驾驶时操作。

### 基础公式

```text
playerPower =
  activeLevel * 6
  + speciesAtk
  + typeBonus
  + itemBonus
  + motionBonus
  + random(0..10)

wildPower =
  wildLevel * 6
  + wildAtk
  + typeBonus
  + rarityBonus
  + random(0..10)
```

胜利条件：

```text
playerPower >= wildPower
```

失败伤害：

```text
damage = max(1, wildPower - playerPower / 2 - playerDef / 2)
```

胜利奖励：

```text
xp = 6 + wildLevel * 4 + wildAtk
gold = random(1..3 + wildLevel / 2)
```

第一版可以先不显示每回合。只显示结果：

- `Won +34xp`
- `Caught Oddish!`
- `Hit -8hp`

### 手动战斗后续版本

停车时可以做“训练战”或“道馆战”，允许玩家选择：

- Attack
- Guard
- Item
- Run

但行驶野战仍保持自动。

## 11. 捕获系统

捕获只在战胜野外宝可梦后触发，概率较低，避免很快收满。

基础公式：

```text
chance =
  catchRate / 8
  + ballBonus
  + statusBonus
  + lowHpBonus
  - ownedPenalty
```

当前自动战斗没有野怪剩余 HP，可用简化：

```text
chance = max(2, catchRate / 8)
```

推荐道具：

| 道具 | 加成 |
|---|---:|
| Poke Ball | +0 |
| Great Ball | +6 |
| Ultra Ball | +12 |
| Lure Ball | GPS fresh 时 +10 |
| Motion Ball | motionScore > 2.0 时 +10 |

重复捕获：

- 已拥有的宝可梦不重复加入。
- 后续可以转成 candy 或金币奖励。

## 12. 进化系统

数据来源：manifest 的 `game.evolve_level` 和 `game.evolve_to`。

规则：

- 升级后检查是否达到进化等级。
- 如果进化目标存在于当前 catalog，则进化。
- 进化后自动设为当前伙伴。
- 旧形态保留为 owned，可在图鉴里查看。

特殊进化先不做：

- 道具进化
- 通信进化
- 亲密度
- 时间段
- 分支进化

后续可简化成“Evolution Stone”商店道具。

## 13. HP、治疗和濒死

### HP

```text
maxHp = speciesBaseHp + level * 4
```

失败时 HP 降低，但最低保留 1，避免驾驶途中突然进入不可玩状态。

### 濒死设计

第一版不做真正 faint，做“疲劳”：

- HP 到 1 时仍可继续旅行。
- 战斗胜率降低。
- Travel 页 HP 显示黄色/红色。
- 建议停车后去 Center 治疗。

后续可做：

- HP 为 1 时捕获率降低。
- 连续失败会自动进入 Camp Center。

### 治疗方式

| 方式 | 效果 | 场景 |
|---|---|---|
| Potion | 回复 `20` 或 `12 + level * 2` | Bag |
| Super Potion | 回复 `50` | Shop |
| Center Heal | 全队回满 | Camp/Center |
| Rest | 当前伙伴少量回复 | 停车 |
| Berry | 小回复或状态解除 | 掉落 |

## 14. 背包系统

小屏幕不适合大库存。第一版用数量型背包。

| 物品 | 类型 | 上限 | 用途 |
|---|---|---:|---|
| Potion | 治疗 | 9 | 回复 HP |
| Poke Ball | 捕获 | 99 | 基础捕获 |
| Great Ball | 捕获 | 99 | 中级捕获 |
| Revive | 恢复 | 9 | 从 1 HP 回复一半 |
| Berry | 消耗 | 9 | 小回复 |
| Escape Rope | 特殊 | 3 | 跳过下一场战斗 |
| Repel | 特殊 | 9 | 降低一段距离遇敌 |
| Lure | 特殊 | 9 | 提高稀有遇敌 |
| Evolution Stone | 进化 | 9 | 特殊进化 |

第一实现建议只做：

- Potion
- Poke Ball
- Repel
- Lure

## 15. 商店系统

商店出现在 Camp/Center，或者作为路上随机事件。

### 常驻商品

| 商品 | 价格 | 解锁 |
|---|---:|---|
| Potion | 10 | 初始 |
| Poke Ball | 8 | 初始 |
| Great Ball | 18 | 当前固件常驻 |
| Repel | 15 | 当前固件常驻 |
| Lure | 20 | 当前固件常驻 |
| Revive | 35 | 伙伴 Lv10 |

### 动态折扣

| 条件 | 效果 |
|---|---|
| GPS fresh 且行驶超过 2km | 商店刷新 |
| 连续失败 2 次 | Potion 折扣 |
| 捕获新宝可梦 | Poke Ball 折扣一次 |
| LoRa 附近有玩家 | 随机商品折扣 |

### 经济来源

- 战斗胜利获得少量金币。
- 首次捕获奖励金币。
- 完成路线任务奖励金币。
- 重复遇到已拥有宝可梦可给 candy 或少量金币。

## 16. 宝可梦中心 / Camp Center

这是停车后的主要交互页面。

功能优先级：

1. Heal team
2. Shop
3. Bag
4. Pokedex
5. Trip summary
6. Settings

### 停车触发

```text
if speedKmph < 2 and motionScore < 0.3 for 15s:
    show Camp Center hint
```

不要强制跳页。行驶中自动跳页会打断玩家。推荐显示提示：

```text
Stopped: Enter Center
```

### Center Heal

- 全体已拥有宝可梦 HP 回满。
- 不收费。
- 可以播放短动画或音效。
- 记录一次治疗次数。

## 17. 图鉴系统

图鉴是本项目的核心界面之一。

### 图鉴状态

| 状态 | 显示 |
|---|---|
| 未见过 | `No.???` 或暗色占位 |
| 见过 | 名字 + 剪影/暗图 |
| 捕获 | 彩色图 + 等级/HP/XP |

当前固件已经有 owned，后续建议加 seen bitset。

### 图鉴页面

网格页：

```text
Pokedex
[img] [img] [?]   [img]
No001 No002 No003 No004
[img] [?]   [img] [img]
No005 No006 No007 No008
Seen 24 Got 9
```

详情页：

```text
Pokedex / Party
[image] No.025 Pikachu
        Electric CAUGHT
        Lv7 XP 18
        HP 42/48
        Seen 32 Got 12/200
Enter select  Space grid
```

后续增强：

- 显示 GIF 动画。
- 显示捕获地点/首次遇见距离。
- 显示进化目标。
- 显示属性克制图标。

## 18. 队伍系统

考虑小屏幕和 NVS，第一版不做传统 6 格队伍，而是：

- 所有已捕获宝可梦都在图鉴中。
- 当前只能选择一个 Active Partner。
- 战斗只使用 Active Partner。

后续队伍版：

| 槽位 | 用途 |
|---|---|
| Lead | 出战 |
| Buddy | 提供被动 |
| Support | 商店/治疗/捕获加成 |

这样保留宝可梦队伍感，但不会让 UI 变成复杂管理器。

## 19. 路线与地区

不需要真实地图瓦片。用传感器推断“路线状态”。

| 路线状态 | 判定 | 玩法 |
|---|---|---|
| Walk | speed < 8 | 遇敌多，等级低 |
| City | 8-35 | 商店/中心事件多 |
| Road | 35-80 | 标准探索 |
| Highway | 80+ | 遇敌少但等级高 |
| Rough | motionScore 高 | 稀有和危险上升 |
| Lost | GPS 差 | 异常事件 |

第一版只用这些状态修正遇敌间隔和等级。后续可做不同 encounter table。

## 20. 任务系统

任务要短，适合真实出行。

### 日常任务

| 任务 | 条件 | 奖励 |
|---|---|---|
| First Kilometer | 行驶 1km | Potion x1 |
| New Friend | 捕获 1 只 | Poke Ball x3 |
| Road Trainer | 战胜 5 场 | Gold |
| Safe Drive | 低 motionScore 行驶 2km | Great Ball |
| Long Route | 单次行驶 10km | 稀有遇敌提升 |

### 长线任务

| 任务 | 条件 | 奖励 |
|---|---|---|
| Kanto Scout | 捕获 25 只 1-151 | Lure |
| Johto Gate | 捕获 10 只 152-200 | Evolution Stone |
| Starter Bond | 初始伙伴 Lv16 | 进化纪念徽章 |
| Signal Trail | LoRa 收到 3 次附近信号 | 特殊商店 |

## 21. LoRa 多机玩法

不作为第一版核心，但设计要留口。

### Beacon

```text
CPK1|device|activeId|level|distance|mode
```

### 效果

| 附近信号 | 效果 |
|---|---|
| 发现训练师 | Travel 页短提示 |
| 等级相近 | 下一战 XP +5% |
| 对方有不同 starter | 商店刷新球类 |
| 多个信号 | Party aura，捕获率 +2 |

不做实时对战。LoRa 用作“附近世界还活着”的轻社交感。

## 22. 音效与视觉

视觉优先级：

1. 64x64 PNG 稳定显示。
2. Travel/Battle/Pokedex 里同一套图像一致。
3. GIF 转轻量 runtime animation。
4. 捕获/进化/升级短动画。
5. 中心治疗动画。

音效优先级：

- 遇敌提示。
- 胜利提示。
- 捕获成功。
- 升级/进化。
- 低电量或 GPS lost 轻提示。

## 23. 存档设计

当前需要支持 200 个 catalog 槽位。

建议 NVS 字段：

| 字段 | 类型 | 用途 |
|---|---|---|
| schema | u32 | 存档版本 |
| owned | bitset | 已捕获 |
| seen | bitset | 已遇见 |
| active | i32 | 当前伙伴 index |
| levels | blob u8[200] | 等级 |
| xp | blob u16[200] | XP |
| hp | blob i16[200] | HP |
| distm | u32 | 总距离 |
| gold | u16 | 金币 |
| potions | u8 | Potion |
| balls | u8 | Poke Ball |
| greatBalls | u8 | Great Ball |
| repelM | u16 | repel 剩余距离 |
| lureM | u16 | lure 剩余距离 |
| caughtCount | u16 | 统计 |
| battleWins | u16 | 统计 |

迁移原则：

- 新字段缺失时用默认值。
- catalog 缩小时不要崩溃。
- catalog 扩到 200 以上前先换二进制资源/存档策略。

## 24. 小屏 UI 规则

- 主屏最多一个主图 + 四到五行文本。
- 每行尽量 24 字符以内。
- 按键提示放底部，短而固定。
- 长列表必须分页或左右浏览。
- 道具名主界面使用短名：`Pot`、`Ball`、`Repel`、`Lure`。
- 金币用 `G`。
- GPS 状态用 `S9 H1.2` 表示卫星和 HDOP。

## 25. 第一阶段实现建议

下一步最值得做的顺序：

1. 增加 `seen` bitset。
2. 增加金币、Potion、Poke Ball 到 NVS。
3. 胜利后消耗 Ball 才能捕获。
4. 增加 Camp Center 页面。
5. Camp Center 支持 Heal team。
6. 增加 Shop 页面，先卖 Potion 和 Poke Ball。
7. Travel 停车提示进入 Center。
8. Pokedex 显示 Seen/Caught 区分。
9. 实现基础属性克制。
10. 增加 Repel/Lure 两个驾驶道具。

## 26. 阶段路线

### Phase 1: Playable Foundation

- 200 只 manifest catalog。
- PNG 图鉴与战斗图。
- GPS/IMU 驱动遇敌和等级。
- 自动战斗、升级、进化、低概率捕获。
- NVS 保存 200 槽。

### Phase 2: Center And Economy

- Camp Center。
- Heal team。
- Shop。
- Bag。
- Gold、Potion、Ball。
- seen/caught 区分。

### Phase 3: Better Pokemon Feel

- 属性克制。当前固件已有轻量倍率。
- 捕获球类型。当前固件已有 Great Ball。
- Repel/Lure。当前固件已有 10 分钟限时效果。
- 更合理的 encounter table。当前固件已有路线状态加权。
- 进化动画。当前固件已有闪烁反馈。

### Phase 4: Animation

- GIF 转 runtime animation。
- Travel idle animation。
- Battle intro/result animation。
- Pokedex large art mode。

### Phase 5: Route Identity

- Walk/City/Road/Highway/Rough/Lost 路线状态。
- 每种路线有不同宝可梦倾向。
- 日常任务和长线任务。
- Trip summary。

### Phase 6: LoRa Social Layer

- 训练师 beacon。
- 附近训练师提示。
- Party aura。
- 特殊商店刷新。

## 27. 当前固件映射

| 当前实现 | 设计含义 | 后续 |
|---|---|---|
| `manifest.json` catalog | 宝可梦资料库 | 保留，必要时换二进制 |
| `owned` bitset | 捕获状态 | 增加 seen |
| `PokemonState` | 单体等级/HP/XP | 增加状态/亲密可选 |
| `distanceM` | 总探索距离 | 增加 trip summary |
| `sessionDistanceM` | 本次距离 | 任务和商店刷新 |
| `motionScore` | 活动强度 | 路线状态和稀有修正 |
| `start_battle()` | 自动野战 | 加属性和道具 |
| `maybe_evolve()` | 等级进化 | 加特殊进化 |
| `render_pokedex()` | 图鉴/队伍 | 加 seen 和大图模式 |

## 28. 成功标准

这个项目做对了的时候，用户体验应该是：

- 上车打开就是自己的宝可梦伙伴。
- 开一段路会自然遇到野怪。
- 到目的地时能看到这段路带来了 XP、捕获和图鉴变化。
- 停车后愿意花 30 秒治疗、买球、看看新图鉴。
- 第二天再开车，会期待今天能遇到什么。
