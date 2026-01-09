from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv
from fsm_melee_env_v2 import FsmMeleeEnvV2

def make_env():
    def _thunk():
        return FsmMeleeEnvV2(dt=0.3, max_steps=240)
    return _thunk

env = DummyVecEnv([make_env() for _ in range(8)])

model = PPO(
    "MlpPolicy",
    env,
    verbose=1,
    device="cpu",
    n_steps=1024,
    batch_size=256,
    gamma=0.98,
)

model.learn(total_timesteps=500_000)
model.save("ppo_fsm_melee_v2")
