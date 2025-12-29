# fsm_melee_env_v2.py
from __future__ import annotations
import numpy as np
import gymnasium as gym
from gymnasium import spaces


class FsmMeleeEnvV2(gym.Env):
    """
    FSM v2 (근접)
    Actions:
      0 Idle
      1 Patrol
      2 Chase
      3 Attack
      4 Return
    """

    ACT_IDLE   = 0
    ACT_PATROL = 1
    ACT_CHASE  = 2
    ACT_ATTACK = 3
    ACT_RETURN = 4

    def __init__(
        self,
        dt: float = 0.3,
        max_steps: int = 240,
        seed: int | None = None,
        force_combat: bool = False,   # ★ 추가: 평가용
    ):
        super().__init__()
        self.dt = dt
        self.max_steps = max_steps
        self.force_combat = force_combat
        self.rng = np.random.default_rng(seed)

        # ===== 월드 =====
        self.world_half = 14.0
        self.sight = 11.0

        # ===== 몬스터(근접) =====
        self.player_speed = 5.0
        self.patrol_mul = 0.50
        self.chase_mul  = 1.10
        self.flee_mul   = 0.70

        self.patrol_speed = self.player_speed * self.patrol_mul
        self.chase_speed  = self.player_speed * self.chase_mul
        self.flee_speed   = self.player_speed * self.flee_mul

        self.attack_range = 1.3
        self.keep_attack_range = 1.8
        self.attack_damage = 10.0
        self.attack_cd_base = 0.9
        self.low_hp_ratio = 0.25

        # ===== 타겟 봇 =====
        self.target_speed = 5.0
        self.target_attack_range = 1.2
        self.target_damage = 6.0
        self.target_attack_cd_base = 1.0

        self.target_flee_dist = 1.4
        self.target_chase_dist = 7.0

        # ===== Observation =====
        self.obs_dim = 16
        self.observation_space = spaces.Box(
            low=-np.inf, high=np.inf, shape=(self.obs_dim,), dtype=np.float32
        )
        self.action_space = spaces.Discrete(5)

        # ===== 상태 =====
        self.step_count = 0
        self.m_pos = np.zeros(2, dtype=np.float32)
        self.m_hp = 100.0
        self.m_max_hp = 100.0
        self.m_attack_cd = 0.0

        self.t_pos = np.zeros(2, dtype=np.float32)
        self.t_hp = 100.0
        self.t_attack_cd = 0.0
        self.target_active = True

        self.patrol_dir = np.array([1.0, 0.0], dtype=np.float32)
        self.patrol_timer = 0.0
        self.last_action = self.ACT_IDLE

    # -------------------------------------------------

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        self.step_count = 0

        self.m_pos = self.rng.uniform(-4, 4, size=2).astype(np.float32)
        self.m_hp = 100.0
        self.m_attack_cd = 0.0

        # ★ 평가(force_combat)면 무조건 타겟 생성
        if self.force_combat:
            self.target_active = True
        else:
            self.target_active = (self.rng.random() >= 0.35)

        if self.target_active:
            self.t_pos = self.rng.uniform(-4, 4, size=2).astype(np.float32)
            if np.linalg.norm(self.t_pos - self.m_pos) < 2.0:
                self.t_pos += np.array([2.5, 0.0], dtype=np.float32)
            self.t_hp = 100.0
            self.t_attack_cd = 0.0
        else:
            self.t_pos = np.array([999.0, 999.0], dtype=np.float32)
            self.t_hp = 0.0
            self.t_attack_cd = 0.0

        self.patrol_dir = self._rand_dir()
        self.patrol_timer = 0.0
        self.last_action = self.ACT_IDLE

        return self._obs(), {}

    # -------------------------------------------------

    def step(self, action: int):
        self.step_count += 1
        self.last_action = int(action)

        self.m_attack_cd = max(0.0, self.m_attack_cd - self.dt)
        self.t_attack_cd = max(0.0, self.t_attack_cd - self.dt)

        has_target = self.target_active
        lowhp = self._lowhp()

        prev_dist = self._dist() if has_target else 0.0
        dealt = 0.0
        taken = 0.0

        # ===== 몬스터 행동 =====
        if action == self.ACT_IDLE:
            pass

        elif action == self.ACT_PATROL:
            if self.patrol_timer <= 0.0:
                self.patrol_timer = float(self.rng.uniform(1.5, 3.5))
                self.patrol_dir = self._rand_dir()
            else:
                self.patrol_timer = max(0.0, self.patrol_timer - self.dt)
            self.m_pos += self.patrol_dir * (self.patrol_speed * self.dt)

        elif action == self.ACT_CHASE:
            if has_target:
                dirn, _ = self._dir_to_target()
                self.m_pos += dirn * (self.chase_speed * self.dt)

        elif action == self.ACT_RETURN:
            if has_target:
                dirn, _ = self._dir_to_target()
                self.m_pos += (-dirn) * (self.flee_speed * self.dt)

        elif action == self.ACT_ATTACK:
            pass

        self.m_pos = np.clip(self.m_pos, -self.world_half, self.world_half)

        # ===== 타겟 =====
        if has_target:
            self._target_bot_move()
            dealt = self._resolve_monster_attack(action)
            taken = self._resolve_target_attack()

        # ===== 보상 =====
        reward = 0.0
        reward += dealt * 0.15
        reward -= taken * 0.08

        if has_target and action == self.ACT_IDLE:
            reward -= 0.01

        if not has_target:
            if action in (self.ACT_PATROL, self.ACT_IDLE):
                reward += 0.002
            else:
                reward -= 0.01

        if lowhp:
            if action == self.ACT_RETURN:
                reward += 0.02
            else:
                reward -= 0.02

        if has_target and lowhp and action == self.ACT_RETURN:
            reward += (self._dist() - prev_dist) * 0.03

        if has_target and action == self.ACT_ATTACK:
            if self._dist() <= self.keep_attack_range:
                reward += 0.01
            else:
                reward -= 0.01

        reward -= 0.001

        terminated = False
        truncated = False
        if self.m_hp <= 0.0:
            terminated = True
            reward -= 1.0
        elif has_target and self.t_hp <= 0.0:
            terminated = True
            reward += 1.0

        if self.step_count >= self.max_steps:
            truncated = True

        return self._obs(), reward, terminated, truncated, {
            "hasTarget": has_target,
            "dealt": dealt,
            "taken": taken,
            "m_hp": self.m_hp,
            "t_hp": self.t_hp,
            "lowhp": lowhp,
        }

    # -------------------------------------------------
    # internals
    def _lowhp(self):
        return (self.m_hp / self.m_max_hp) <= self.low_hp_ratio

    def _dist(self):
        return float(np.linalg.norm(self.t_pos - self.m_pos))

    def _dir_to_target(self):
        v = self.t_pos - self.m_pos
        d = float(np.linalg.norm(v))
        if d < 1e-6:
            return np.zeros(2, dtype=np.float32), 0.0
        return (v / d).astype(np.float32), d

    def _rand_dir(self):
        a = float(self.rng.uniform(0.0, 2.0 * np.pi))
        return np.array([np.cos(a), np.sin(a)], dtype=np.float32)

    def _obs(self):
        o = np.zeros((self.obs_dim,), dtype=np.float32)
        if self.target_active:
            dirn, dist = self._dir_to_target()
            o[0] = 1.0
            o[1] = np.clip(dist / self.sight, 0.0, 2.0)
            o[2] = dirn[0]
            o[3] = dirn[1]
            o[7] = 1.0 if dist <= self.attack_range else 0.0
            o[8] = 1.0 if dist <= self.keep_attack_range else 0.0
            o[6] = 1.0 if (self.m_attack_cd <= 0.0 and o[7] > 0.5) else 0.0
        else:
            o[0] = 0.0

        o[4] = np.clip(self.m_hp / self.m_max_hp, 0.0, 1.0)
        o[5] = 1.0 if self._lowhp() else 0.0

        base = 9
        o[base + self.last_action] = 1.0
        return o

    def _target_bot_move(self):
        dirn, dist = self._dir_to_target()
        v = np.zeros(2, dtype=np.float32)
        if dist < self.target_flee_dist:
            v = dirn
        elif dist > self.target_chase_dist:
            v = -dirn
        self.t_pos += v * (self.target_speed * self.dt)
        self.t_pos = np.clip(self.t_pos, -self.world_half, self.world_half)

    def _resolve_monster_attack(self, action):
        dealt = 0.0
        if action == self.ACT_ATTACK and self.m_attack_cd <= 0.0:
            if self._dist() <= self.attack_range:
                self.m_attack_cd = self.attack_cd_base
                self.t_hp -= self.attack_damage
                dealt = self.attack_damage
        self.t_hp = max(0.0, self.t_hp)
        return dealt

    def _resolve_target_attack(self):
        taken = 0.0
        if self.t_attack_cd <= 0.0:
            if self._dist() <= self.target_attack_range:
                self.t_attack_cd = self.target_attack_cd_base
                self.m_hp -= self.target_damage
                taken = self.target_damage
        self.m_hp = max(0.0, self.m_hp)
        return taken
