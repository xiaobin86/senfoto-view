-- SFQ96A Main Data Stream Protocol for RS16
robosense_difop_proto = Proto("SFQ96ADIFOP","SFQ96A Device Info Output Protocol (DIFOP)")

function dissect_angle_calibration(buffer,subtree)
	local nbChannel = 16
	local curr = 0
	for i=1, nbChannel
	do
		local channelBuffer = buffer(curr, 3)
		local value = channelBuffer(0, 3):uint() * 0.01
		value = ((i < 8) and (value * -1) or value)
		subtree:add(channelBuffer(0, 3), "Channel " .. i .. " Angle Value: " .. value)
		curr = curr + 3
	end
end


function robosense_difop_proto.dissector(buffer,pinfo,tree)
	pinfo.cols.protocol = "SFQ96ADIFOP"

	local subtree = tree:add(robosense_difop_proto,buffer(),"SFQ96A Device Info Output Protocol (DIFOP):" ..buffer:len())

  -- Check packet size --
	local goodSize = 1248
	local totalSize = buffer:len()
	if totalSize ~= goodSize then return end

    -- Packet Header --
	-- local headerBuffer = buffer(0, 8)
	-- local header_subtree = subtree:add(headerBuffer, "Header")
	-- DIFOP Identifier
	-- 0xA5, 0xFF, 0x00, 0x5A, 0x11, 0x11, 0x55, 0x55

	local motorSpeedBuffer = buffer(8, 2)
	subtree:add(motorSpeedBuffer, "RPM: " .. motorSpeedBuffer:uint())

    -- Ethernet Register (10, 22)

	local fovBuffer = buffer(32, 4)
	local fov_subtree = subtree:add(fovBuffer, "Field of View")
	fov_subtree:add(fovBuffer(0, 2), "FOV Start: " .. fovBuffer(0, 2):uint())
	fov_subtree:add(fovBuffer(2, 2), "FOV End: " .. fovBuffer(2, 2):uint())

    -- Reserved (36, 2)

	local motoPhaseLocking = buffer(38, 2)
	subtree:add(motoPhaseLocking, "Motor Phase Locking: " .. motoPhaseLocking:uint())

    -- Mainboard Firmware Version (40, 5)
	-- Bottom Board Firmware Version (45, 5)
	-- Bottom Board Software Version (50, 5)
	-- Motor Firmware Version (55, 5)
	-- Reserved (60, 232)
	-- Serial Number (292, 6)
	-- Reserved (298, 2)

	local returnMode = buffer(300, 1)
	subtree:add(returnMode, "Return Mode: " .. returnMode:uint())

	-- local timeSync = buffer(301, 2)
	-- local timSync_subtree = subtree:add(timeSync, "Time Synchronization")
	-- timSync_subtree:add(timeSync(0, 1), "Time Sync Mode: " .. timeSync(0, 1):uint())
	-- timSync_subtree:add(timeSync(1, 1), "Sync State: " .. timeSync(1, 1):uint())

	-- Time (303, 10)
	-- Operating Status (313, 18)
	-- Reserved (331, 17)
	-- Fault Diagnosis (342, 18)
	-- Reserved (360, 22)
	-- GPRMC Data (382, 86)
	--     The GPRMC data packet reserves 86 bytes to store the received GPRMC message,
    --     which is output from the external GPS module. The data can be parsed and viewed in
    --     ASCII code.

	-- Vertical Angle Calibration (not applicable for RS16)
	-- local VAC = buffer(468, 96)
	-- local VAC_subtree = subtree:add(VAC, "Vertical Angle Calibration")
	-- dissect_angle_calibration(VAC, VAC_subtree)

	-- Horizontal Angle Calibration (not applicable for RS16)
	-- local HAC = buffer(564, 96)
	-- local HAC_subtree = subtree:add(HAC, "Horizontal Angle Calibration")
	-- dissect_angle_calibration(HAC, HAC_subtree)

	-- Reserved (660, 586)

	-- Pitch Calibration
	local PAC = buffer(1165, 48)
	local PAC_subtree = subtree:add(PAC, "Pitch Angle Calibration")
	dissect_angle_calibration(PAC, PAC_subtree)

	-- Frame End (1246, 2) - should be 0x0F, 0xF0
end


-- load the udp.port table
udp_table = DissectorTable.get("udp.port")
-- register our protocol to handle udp port
udp_table:add(7788,robosense_difop_proto)
