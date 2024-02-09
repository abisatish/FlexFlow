./build/inference/spec_infer/spec_infer -ll:gpu 4 -ll:fsize 14000 -ll:zsize 30000 -llm-model meta-llama/Llama-2-7b-hf -cache-folder /home/xiaoxias/weights  -ssm-model JackFram/llama-68m -prompt /home/xiaoxias/serving/inference/prompt/chatgpt.json -tensor-parallelism-degree 4 --fusion > llama2-speciner_cuda_graph.log 2>&1 