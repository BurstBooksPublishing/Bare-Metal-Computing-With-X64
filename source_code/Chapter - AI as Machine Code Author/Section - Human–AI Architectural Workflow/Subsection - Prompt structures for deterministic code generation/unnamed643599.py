def build_deterministic_prompt(
    cpu_snapshot: str,
    constraints: list[str],
    encoding: str,
    verify_items: list[str],
    failure_policy: str
) -> str:
    prompt = (
        "END PROMPT\n"
        "CPU state:\n" + cpu_snapshot + "\n"
        "Constraints:\n" + "\n".join(constraints) + "\n"
        "Encoding: " + encoding + "\n"
        "Verify:\n" + "\n".join(verify_items) + "\n"
        "Failure policy: " + failure_policy + "\n"
        "Generate x64 machine code. Output only canonical hex bytes.\n"
        "END PROMPT\n"
    )
    return prompt

def send_deterministic_request(prompt, model_client):
    """
    Send request with deterministic decoding hyperparameters.
    model_client must supply a deterministic 'complete' method.
    """
    # Deterministic decoding: temperature=0, top_k=1, top_p=1, deterministic_seed fixed.
    response = model_client.complete(
        prompt=prompt,
        temperature=0.0,
        top_k=1,
        top_p=1.0,
        max_tokens=512,
        deterministic_seed=42  # if client supports seed for model-determinism
    )
    return response