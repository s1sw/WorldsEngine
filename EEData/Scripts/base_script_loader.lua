-- Base script runner for Worlds Engine.
-- Handles storage of script environments and running of the scripts
-- simply because it's easier to do in Lua than in C++.

envStorage = {}

function storeEnv(env, envKey)
	envStorage[envKey] = env
end

function delEnv(envKey)
	envStorage[envKey] = nil
end

function run(code, fileName, envKey)
	local func, message = load(code, file_name, 't', envStorage[envKey])
	
	if not func then 
		print(message) 
		return 
	end
	
    local success, err = pcall(func)
	if not success then
		logError("script error: ", err)
	end
end