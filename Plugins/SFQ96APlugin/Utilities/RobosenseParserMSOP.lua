-- SFQ96A Main Data Stream Protocol for RS16
robosense_msop_proto = Proto("SFQ96AMSOP","SFQ96A Main Data Stream Protocol (MSOP)")

function robosense_msop_proto.dissector(buffer,pinfo,tree)
	pinfo.cols.protocol = "SFQ96AMSOP"

	local subtree = tree:add(robosense_msop_proto,buffer(),"SFQ96A Main Data Stream Protocol (MSOP):" ..buffer:len())

	local curr = 0

  -- Check packet size --
	local goodSize = 1248
	local totalSize = buffer:len()
	if totalSize ~= goodSize then return end

    -- Packet Header --
    local headerSize = 42
	local header_subtree = subtree:add(buffer(curr,headerSize),"Header")

    -- Reserved
    curr = curr + 20

    local timestampSize = 10
	local YearType = buffer(curr, 1):uint()
	header_subtree:add(buffer(curr, 1),"Year : " .. YearType)
	local MonthType = buffer(curr + 1, 1):uint()
	header_subtree:add(buffer(curr + 1, 1),"Month : " .. MonthType)
	local DayType = buffer(curr + 2, 1):uint()
	header_subtree:add(buffer(curr + 2, 1),"Day : " .. DayType)
	local HourType = buffer(curr + 3, 1):uint()
	header_subtree:add(buffer(curr + 3, 1),"Hour : " .. HourType)
	curr = curr + timestampSize

    -- Reserved (probably old lidar type)
    curr = curr + 1

    local lidarTypeSize = 1
	local LidarType = buffer(curr, lidarTypeSize):uint()
	header_subtree:add(buffer(curr, lidarTypeSize),"LidarType : " .. LidarType)
	curr = curr + lidarTypeSize

    local lidarModelSize = 1
	local LidarModel = buffer(curr, lidarModelSize):uint()
	header_subtree:add(buffer(curr, lidarModelSize),"LidarModel : " .. LidarModel)
	curr = curr + lidarModelSize

    -- Reserved
	curr = curr + 7

    local temperatureSize = 2
	local Temperature = buffer(curr, temperatureSize):uint()
	header_subtree:add(buffer(curr, temperatureSize),"Temperature : " .. Temperature)
	curr = curr + temperatureSize

	---- Data Blocks ----

	local nbDataBlock = 12
	local sizeDataBlock = 100
	local nbChannel = 32
    local channelSize = 3
	local DataBlockSize = nbDataBlock * nbChannel * channelSize + 4

	local datablocks = subtree:add(buffer(curr, DataBlockSize),"Data blocks")

	for i=1, nbDataBlock
	do
		local dataBlock_subtree = datablocks:add(buffer(curr, sizeDataBlock),"Data Block " ..i)

        -- Flag always 0xff, 0xee
		curr = curr + 2

		local Azimuth = buffer(curr,2):uint()
		dataBlock_subtree:add(buffer(curr,2),"Azimuth  : " .. Azimuth)
		curr = curr + 2

		local lasers = dataBlock_subtree:add(buffer(curr, channelSize * nbChannel),"Channels")

		---- Channels  ----
		for c=0, nbChannel-1
		do
            -- From 1 to 16 first return, 17 to 32 second return data for same channels
			numChannel = c
			local laser_subtree = lasers:add(buffer(curr, 3),"Channel " ..numChannel)

			local Distance = buffer(curr,2):uint()
			laser_subtree:add(buffer(curr,2),"Distance  : " .. Distance)
			curr = curr + 2

			local Reflectivity = buffer(curr,1):uint()
			laser_subtree:add(buffer(curr,1),"Reflectivity  : " .. Reflectivity)
			curr = curr + 1
		end

	end

	-- Footer --

	local footer_subtree = subtree:add(buffer(curr,6),"Footer")

    -- Reserved
	curr = curr + 2

    local IndexSize = 2
	local Index = buffer(curr, IndexSize):uint()
	footer_subtree:add(buffer(curr, IndexSize),"Index : " .. Index)
	curr = curr + IndexSize

    -- End bytes info always 0x00, 0xFF
	curr = curr + 2

end


-- load the udp.port table
udp_table = DissectorTable.get("udp.port")
-- register our protocol to handle udp port
udp_table:add(6699,robosense_msop_proto)
