# W
# 172.23.176.1

extends Node2D

@onready var truck = $Lead
@onready var follower_template = $Follower

# movement variables
var offset := PI/2
var speed := 800.0
var turn_speed := 90
var rotation_threshold := 5
var distance := 180
var platoon = []
var position_histories = []
var rotation_histories = []
var history_length := 20
var target_rotation := 0.0

# physics state variables
var velocity := 0.0
var heading := 0.0

#tcp variables
var tcp_server: TCPServer
var client: StreamPeerTCP
var server_port := 9999

#network input state
var network_accel := 0.0
var network_heading := 0.0

# preparation of the scene when hitting run
func _ready():
	follower_template.visible = false
	platoon.append(truck)
	position_histories.append([])
	rotation_histories.append([])
	setup_server()
	target_rotation = truck.rotation

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	
	# check for new client connections here
	if tcp_server.is_connection_available():
		if client and client.get_status() == StreamPeerTCP.STATUS_CONNECTED:
			client.disconnect_from_host()
		client = tcp_server.take_connection()
		print("Client connected from: ", client.get_connected_host())
	
	# process the network data here
	if client and client.get_status() == StreamPeerTCP.STATUS_CONNECTED:
		if client.get_available_bytes() > 0:
			var data = client.get_utf8_string(client.get_available_bytes())
			process_network_command(data)
	
	
	if Input.is_action_just_pressed("ui_accept"):
		spawn_follower()
	
	##physics integration - velocity update here
	#velocity += network_accel * delta
	#velocity = clamp(velocity, 0, speed)
	#
	## friction drag to slow down
	#if network_accel == 0:
		#velocity = lerp(velocity, 0.0, 0.5 * delta)
		#
	## physics integration heading update here
	#var target_heading_rad = deg_to_rad(network_heading)
	#heading = lerp_angle(heading, target_heading_rad, turn_speed * delta)
	#
	## movement direction calculation
	#var direction = Vector2.RIGHT.rotated(heading)
	#
	## update truck position and rotation
	#truck.position += direction * velocity * delta
	#truck.rotation = heading + offset
	#
			
	for i in range(platoon.size()):
		position_histories[i].append(platoon[i].position)
		rotation_histories[i].append(platoon[i].rotation)
		
		if position_histories[i].size() > history_length:
			position_histories[i].pop_front()
			rotation_histories[i].pop_front()
		
	update_all_followers(delta)
	


# processing of movement, link/delink, and many other commands 
func process_network_command(data: String):
	
	# parsing commands and applying the correct one
	var commands = data.split("\n", false)
	
	for cmd in commands:
		cmd = cmd.strip_edges()
		
		# checking for adding trucks
		if cmd.to_upper().begins_with("LINK"):
			var parts = cmd.split(" ", false)
			if parts.size() >= 2:
				spawn_follower()
			continue
			
		# checking for removing trucks
		if cmd.to_upper().begins_with("DELINK"):
			var parts = cmd.split(" ", false)
			if parts.size() >= 2:
				var position = int(parts[1])
				remove_follower(position)
			continue
			
		# parsing for STATE commands
		if cmd.to_upper().begins_with("STATE"):
			var parts = cmd.split(" ", false)
			if parts.size() >= 5:
				var new_x = float(parts[1]) * 1.8
				var new_y = float(parts[2]) * 1.8
				var new_heading = float(parts[3])
				
				truck.position = Vector2(new_x, new_y)
				truck.rotation = deg_to_rad(new_heading) + offset
				print("Heading: ", new_heading, " Rotation: ", truck.rotation)
			continue

# setting up server in ready function
func setup_server():
	tcp_server = TCPServer.new()
	var err = tcp_server.listen(server_port)
	if err == OK:
		print("TCP Server started on port ", server_port)
	else: 
		print("Failed to start server ", server_port)

# spwaning the follower on receiving LINK command
func spawn_follower():
	
	var leader_truck = platoon[platoon.size() - 1]
	#var leader_idx = platoon.size() - 1
	
	var new_follower = follower_template.duplicate()
	add_child(new_follower)
	
	var facing_angle = leader_truck.rotation - PI/2
	var facing_direction = Vector2.RIGHT.rotated(facing_angle)
	var spawn_pos = leader_truck.position - facing_direction * distance
	
	new_follower.position = spawn_pos
	new_follower.rotation = leader_truck.rotation
	new_follower.visible = true
	
	platoon.append(new_follower)
	
	var new_position_history = []
	var new_rotation_history = []
	for i in range (history_length):
		new_position_history.append(leader_truck.position)
		new_rotation_history.append(leader_truck.rotation)
	
	position_histories.append(new_position_history)
	rotation_histories.append(new_rotation_history)
	
	print("Spawned follower #", platoon.size() - 1, " Total trucks: ", platoon.size())
	

# despawning the relevant follower on receiving DELINK command
func remove_follower(position: int):
	if position < 2 or position > platoon.size():
		print ("Invalid position to remove: ", position)
		return
	
	var follower_idx = position - 1
	var follower = platoon[follower_idx]
	
	platoon.remove_at(follower_idx)
	position_histories.remove_at(follower_idx)
	rotation_histories.remove_at(follower_idx)
	
	follower.queue_free()
	
	print("Removed follower at position ", position, ". Total trucks: ", platoon.size())

func update_all_followers(delta: float):
	
	for i in range(1, platoon.size()):
		var follower_truck = platoon[i]
		var leader_idx = i - 1
	
		# using delayed position from history
		if position_histories[leader_idx].size() > 0:
			var delayed_position = position_histories[leader_idx][0]
			var delayed_rotation = rotation_histories[leader_idx][0]
		
			var current_distance = platoon[leader_idx].position.distance_to(follower_truck.position)
			var smooth_factor = 12.0
		
			if current_distance > distance:
				follower_truck.position = follower_truck.position.lerp(delayed_position, smooth_factor * delta)
			follower_truck.rotation = lerp_angle(follower_truck.rotation, delayed_rotation, smooth_factor * delta)
		
	#
			# rotation smoothing
			#follower_truck.rotation = lerp_angle(follower_truck.rotation, delayed_rotation, 8.0 * delta)
		

func _exit_tree():
	if client:
		client.disconnect_from_host()
	if tcp_server:
		tcp_server.stop()
