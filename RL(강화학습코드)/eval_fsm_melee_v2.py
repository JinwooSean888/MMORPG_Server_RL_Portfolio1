from stable_baselines3 import PPO
from fsm_melee_env_v2 import FsmMeleeEnvV2

ACTION = ["Idle","Patrol","Chase","Attack","Return"]

if __name__ == "__main__":
    env = FsmMeleeEnvV2(dt=0.3, max_steps=240)
    model = PPO.load("ppo_fsm_melee_v2")

    obs, _ = env.reset()
    total = 0.0
    for i in range(240):
        action, _ = model.predict(obs, deterministic=True)
        obs, reward, term, trunc, info = env.step(int(action))
        total += reward
        if i % 10 == 0:
            d = env._dist() if env.target_active else -1
            print(i, ACTION[int(action)], "dist=", d, info)
        if term or trunc:
            break
    print("episode reward:", total)
