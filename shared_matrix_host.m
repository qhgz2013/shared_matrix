classdef shared_matrix_host < handle
    properties (GetAccess = public, SetAccess = private)
        Name
        % HOST does not need to keep a data matrix (it's written to shared memory)
        Handle
        BasePointer
        IsAttached
        Platform
    end
    
    methods
        function obj = shared_matrix_host(input_variable)
            obj.Name = char(java.util.UUID.randomUUID);
            obj.Platform = test_platform();
            if obj.Platform == 0
                error('SharedMatrix:NotSupported', 'Underlying MEX API not supported');
            elseif obj.Platform == 1
                obj.Name = ['Local\' obj.Name];
            end
            [obj.BasePointer, obj.Handle] = create_shared_matrix(obj.Name, input_variable);
            obj.IsAttached = true;
        end
        
        function copy = attach(obj)
            if ~obj.IsAttached
                error('SharedMatrix:DataDetachedError', 'Shared memory has been detached');
            end
            copy = shared_matrix(obj.Name, obj.Platform);
        end
        
        function detach(obj)
            if obj.IsAttached
                obj.IsAttached = false;
                if obj.Platform == 1
                    delete_shared_matrix(obj.Handle, obj.BasePointer);
                elseif obj.Platform == 2
                    delete_shared_matrix(obj.Name, obj.BasePointer);
                end
            end
        end
        
        function delete(obj)
            obj.detach();
        end
    end
end

