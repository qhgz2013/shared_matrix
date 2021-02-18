classdef shared_matrix < handle
    properties (GetAccess = public, SetAccess = private)
        Name
        CellArray
        Handle
        BasePointer
        IsAttached
        Platform
    end
    
    methods
        function obj = shared_matrix(name, platform)
            obj.Name = name;
            obj.IsAttached = false;
            obj.Handle = uint64(0);
            obj.BasePointer = uint64(0);
            obj.CellArray = [];
            obj.Platform = platform;
        end
        
        function arr = get_data(obj)
            if ~obj.IsAttached
                obj.CellArray = cell(1);
                [obj.CellArray{1}, obj.Handle, obj.BasePointer] = read_shared_matrix(obj.Name);
                arr = obj.CellArray{1};
                obj.IsAttached = true;
            else
                arr = obj.CellArray{1};
            end
        end
        
        function detach(obj)
            if obj.IsAttached
                obj.IsAttached = false;
                delete_shared_matrix(obj.Handle, obj.BasePointer, obj.CellArray, obj.Name);
            end
        end
        
        function delete(obj)
            obj.detach();
        end
    end
end
